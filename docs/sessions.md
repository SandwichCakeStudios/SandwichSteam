---
title: Sessions
parent: Multiplayer
nav_order: 1
---

# Sessions
{: .no_toc }

Host a match, let other players find it, join it and travel there together. A session is one match. Under the hood it is a Steam lobby plus a listen server on the host's machine, with game traffic through Steam (SteamSockets).

> **Status:** create, host travel, find, join and travel are tested on two machines. Invites, *Join Game* from the friends list, kick, owner migration and lobby chat are **not yet tested** with two accounts.

1. TOC
{:toc}

## Before you begin

- The [Quick Start](quick-start.html) is done: App ID set, **Configure Steam...** run, an App Definition assigned.
- **Run Configure Steam... again** if you installed the plugin before you used sessions. With the Sessions feature installed it also writes the network lines (SteamSockets net driver). Restart the editor afterwards.
- **Two machines and two Steam accounts** for a real test. One PC can host and search, but cannot join itself. The accounts should be friends for invites.
- **The same build on both machines.** Steam only finds sessions of the same build, so package once and copy it.

Read [Core Concepts](core-concepts.html) first if you have not: tags, the App Definition and async nodes are used everywhere below.

## Session profiles

A **session profile** describes one kind of match, for example *Coop* or *Versus*. Add one per kind of match to the **Sessions** list of your App Definition.

1. Add a tag under `Steam.Session` in **Project Settings > Gameplay Tags**, for example `Steam.Session.Coop`.
2. Open your App Definition, add a row to **Sessions** and pick the tag.
3. Set the fields you care about. The defaults work.

| Field | What it does |
|---|---|
| **Tag** | Picks the profile in the nodes. Required |
| **Max Players (limit)** | Most players, host included (up to 250) |
| **Min Players**, **Default Players** | Lower limit and the preset for a host menu. 0 = no limit / Max Players |
| **Default Visibility** | **Public** (listed in searches), **Friends Only** (friends list and invites) or **Private** (invite only) |
| **Allowed Visibilities** | Which visibilities a player may pick. None checked = all |
| **Uses Presence** | Keep it on. Off makes a session that searches, invites and *Join Game* cannot see |
| **Allow Join In Progress** | Players may join after the match started |
| **Fixed Settings** | Key / value pairs always set, for example `mode = coop`. Searches can filter on them |
| **Player Settings** | Keys the player may change in a host menu, with their default values |

The App Definition checks the rows for you (tags present and unique, player counts in order) and reports problems when you save.

## Host a match

The host creates the session, then travels to the match map with **listen** on. Everyone who joins travels to the host.

1. **Create Steam Session** with your profile tag.
2. **On Success:** **Steam Server Travel** with the match map and **Listen** checked.
3. **On Failure:** show the *Error Tag* or *Message*. Common reasons: already in a session, unknown tag.

<!-- SHOT 60: Blueprint: Create Steam Session (Steam.Session.Coop) -> On Success -> Steam Server Travel (Map, Listen) -->

When the match is over, the host calls **Destroy Steam Session** and travels back to the menu map. Clients get **On Session Lost** with `Steam.Error.Session.HostLeft`.

> **Order matters:** the host travels first, then the clients join. A client that joins before the host has travelled sits in the lobby until it travels, which is the [party](party-lobby.html) flow, not this one.

## Find and join

1. **Find Steam Sessions** with *Options*:
   - **Profile Tag**: only sessions of this profile. Recommended, especially on Spacewar (480).
   - **Max Results** (default 20), **Distance** (**Close**, **Default**, **Far**, **Worldwide**).
   - **Filters**: only sessions whose setting has exactly this value, for example `mode = coop`.
   - **Name Contains**: only sessions whose name contains this text.
2. **On Sessions Found** gives an array of results. Show them in a list: **Display Name**, **Owner Name**, **Ping Ms**, **Open Slots** / **Max Players**.
3. When the player picks one: **Join Steam Session** with that result.
4. **On Joined:** the plugin travels to the host by itself (setting *Auto Travel After Join*).

<!-- SHOT 61: Blueprint: Find Steam Sessions -> On Sessions Found -> For Each -> server list widget; Join Steam Session from a button -->

Join failures you should handle: `Steam.Error.Lobby.Full` and `Steam.Error.Lobby.NotFound` (the session closed since the search: search again).

## Invites and Join Game

- **Show Steam Invite Overlay** opens Steam's invite window. **Send Steam Session Invite** invites one friend by Steam ID.
- A friend who accepts joins your session and travels to you. This also works when the friend's game is not running: Steam starts it and the plugin joins once the first player controller exists.
- For **Join Game** on the friends list, set the rich presence connect string (Presence nodes) to `+connect_lobby ` followed by **Get Joinable Steam Lobby Id**, after creating or joining the session.

### When the player is busy

Every request from Steam (invite, *Join Game*, launch) fires **On Join Requested** on the **Steam Sessions Subsystem**. Its *Action* says what the plugin does:

| Action | Meaning |
|---|---|
| Join Now | Joins right away |
| Leave And Join | Leaves the current session, then joins |
| Ask Game | Waits for you: show a "Join your friend?" prompt, then call **Accept Steam Join Request** or **Decline Steam Join Request** |
| Ignore | Already in that session, or the policy says ignore |

The settings *Auto Join Requests* and *Join While In Match* decide which action you get. **Get Pending Steam Join Request** reads a waiting request, for a UI that opens later.

## Let players choose the settings

For a host menu (name, player count, visibility, game mode):

1. **Make Steam Session Settings From Profile** gives the profile's defaults as **Steam Session Settings**: *Display Name*, *Max Players*, *Visibility*, *Allow Join In Progress*, *Settings*.
2. Let the player edit them in your widget.
3. **Create Steam Session From Profile** with the profile tag and the edited settings. **On Created** returns the *Applied Settings*.

The profile's rules apply: numbers are clamped into range, a choice the profile does not allow (a visibility, a fixed key) fails with `Steam.Error.InvalidArgument`. It never quietly makes a session more public than asked.

- **Create Steam Session With Settings** needs no profile at all: full control, only Steam's limits apply.
- **Update Steam Session** changes name, visibility, slots or settings while the session runs. Host only.
- **Get Current Steam Session Settings** reads what is applied right now.

> The profile rules are checked on the host. They are guardrails for your UI, not anti-cheat. To have Steam check every player who joins, see [Steam Authentication](steam-authentication.html).

## When the session ends by itself

**On Session Lost** (on the **Steam Sessions Subsystem**) fires when the session ended without the player asking. The *Reason*:

| Reason | Meaning |
|---|---|
| `Steam.Error.Session.HostLeft` | The host quit or closed the session |
| `Steam.Error.Session.Timeout` | The connection to the host timed out |
| `Steam.Error.Session.Kicked` | The host removed this player |
| `Steam.Error.Failed` | Any other network or travel failure |

The plugin leaves the session for you, and Unreal returns the player to the default map. Show a message there. If the player is in a [party](party-lobby.html) (**Is In Steam Party**), show the party screen instead.

## Lobby chat, ready check and data

Every session is also a Steam lobby, so players can chat and get ready before and during the match:

| Node | What it does |
|---|---|
| **Send Steam Lobby Chat Message** | Chat. Everyone, including you, gets **On Lobby Chat Received** |
| **Set Steam Lobby Ready**, **Are All Steam Lobby Members Ready** | A ready check |
| **Get Steam Lobby Members** | Members with name, owner, local and ready flags |
| **Set Steam Lobby Data**, **Get Steam Lobby Data** | Shared key / value data, for example the map. Only the owner writes |
| **Set Steam Lobby Member Data**, **Get Steam Lobby Member Data** | Data per member, for example a chosen character |
| **Set Steam Lobby Joinable** | Close the session to new players. Owner only |
| **Kick Steam Lobby Member**, **Forgive Steam Lobby Member** | Remove a player (and keep them out until forgiven). Owner only |

Events on the subsystem: **On Lobby Chat Received**, **On Lobby Member Changed**, **On Lobby Member Data Changed**, **On Lobby Data Changed**, **On Lobby Owner Changed**, **On Lobby Kicked**.

These nodes work on the [party lobby](party-lobby.html) too: the advanced **Target** pin picks *Active*, *Party* or *Session*. Without a party, *Active* is the session.

## Nodes

| Node | What it does |
|---|---|
| **Create Steam Session** (also *With Settings*, *From Profile*) | Creates a session. One at a time |
| **Find Steam Sessions** | Searches sessions. **On Sessions Found** |
| **Join Steam Session** | Joins a search result. **On Joined** with the connect string |
| **Update Steam Session** | Changes the running session. Host only |
| **Destroy Steam Session** | Leaves or closes the session. Succeeds at once when there is none |
| **Steam Server Travel** | Host: travel to a map (with **Listen** for a match) |
| **Travel To Steam Session** | Client: travel to the host yourself, when *Auto Travel After Join* is off |
| **Is In Steam Session**, **Get Steam Session Lobby Id** | Is the player in a session? |
| **Show Steam Invite Overlay**, **Send Steam Session Invite** | Invite friends |
| **Get Pending Steam Join Request**, **Accept Steam Join Request**, **Decline Steam Join Request** | Answer a join request that waits for the game |
| **Make Steam Session Settings From Profile**, **Get Steam Session Profile**, **Get Current Steam Session Settings** | Building blocks for a host menu |

Events on the **Steam Sessions Subsystem**: **On Join Requested**, **On Session Join Finished** (any join, also from an invite), **On Session Lost**, plus the lobby events above.

## Settings

**Project Settings > Plugins > Sandwich Steam - Sessions**

| Setting | Default | What it does |
|---|---|---|
| Join While In Match | Ask Game | A request while already in a session: **Auto Leave**, **Ask Game** or **Ignore** |
| Auto Join Requests | on | Join requests by themselves when not in a session. Off = always **Ask Game** |
| Auto Travel After Join | on | Travel to the host as soon as the join succeeds. Off = call **Travel To Steam Session** yourself, for example after a loading screen |
| Duplicate Request Seconds | 10 | A second request for the same lobby within this time is dropped (Steam often reports one invite twice) |
| Launch Join Wait Seconds | 30 | How long a cold start join waits for the first player controller |
| Apply Max Players To Game Session | on | The host's game mode refuses players past the applied *Max Players*. Turn off if you set it yourself |

The *Party* settings are explained in [Party Lobby](party-lobby.html#settings).

## Console commands

In Standalone or a Development build:

| Command | What it does |
|---|---|
| `Steam.Sessions.Dump` | Session, lobby, members, applied settings |
| `Steam.Sessions.Create <ProfileTag>` | Creates a session |
| `Steam.Sessions.Travel <Map>` | Host travel with listen |
| `Steam.Sessions.Find [ProfileTag]` | Searches and logs a numbered list |
| `Steam.Sessions.Join <index>` | Joins a result of the last search |
| `Steam.Sessions.Destroy` | Leaves the session |
| `Steam.Sessions.InviteOverlay` | Opens the invite window |

A quick two-machine test: **A** runs `Steam.Sessions.Create Steam.Session.Coop`, then `Steam.Sessions.Travel <YourMap>`. **B** runs `Steam.Sessions.Find`, then `Steam.Sessions.Join 0`. B lands in A's map.

The test panel (`Steam.Test.Toggle`) has a **Sessions** page with buttons for all of this.

## Good to know

- **Spacewar (480) is shared by every developer.** A search also returns other people's lobbies, which are thrown away, so your list can be short or empty. Search with a **Profile Tag**, or test with your own App ID.
- **Travel fails, or the client cannot connect?** Look in the log for `Mounting Engine plugin SteamSockets`. Without it, the game falls back to normal IP networking, which cannot reach a Steam host. See [Troubleshooting](troubleshooting.html#sessions-the-join-works-but-the-client-never-arrives).
- **One session at a time.** Create, join and destroy run one after another; a second one while another runs fails with `Steam.Error.Failed`.
- **Dedicated servers are not supported** yet. The host is always a player (listen server).
- When the game shuts down or Steam goes offline, the plugin leaves the session, so no empty lobby stays behind.
- Your own net drivers (beacons and so on): add them after the lines Configure Steam writes in `DefaultEngine.ini`.
