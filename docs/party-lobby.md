---
title: Party Lobby
parent: Multiplayer
nav_order: 2
---

# Party Lobby
{: .no_toc }

Keep a group of friends together between matches. The friends sit in one Steam lobby, the **party**. The party owner starts a match, everyone follows, and when the match ends they are all still in the party, ready for the next one.

You do not have to use it: games with only sessions, or only a lobby, work as before.

1. TOC
{:toc}

## The idea in one picture

```
Party lobby (stays the whole evening)
 ├─ match 1: owner hosts a session, members join it, play, session ends
 ├─ back in the party: chat, ready check, pick a mode
 └─ match 2: ...
```

The **party lobby** is the lobby you make with **Create Steam Lobby** (or join with **Join Steam Lobby**). A match is a normal **session**. A player can be in the party and in a session at the same time.

## Blueprint flow

**Owner (the one who made the party):**

1. **Create Steam Lobby** (a session profile, or **With Settings**). This is the party.
2. Friends join it (see *Invites* below). Use **Set Steam Lobby Ready** and **Are All Steam Lobby Members Ready** for a ready check, **Send Steam Lobby Chat Message** for chat.
3. When everyone is ready: **Steam Server Travel** to the match map with **Listen** on.
4. On the match map: **Host Steam Party Session** (a session profile). It creates the session and tells the party where the match is.
5. When the match is over: **Destroy Steam Session** and travel back to your party map. The party match ends by itself (setting *Auto End Party Match*).

**Members:** nothing to do. With *Auto Follow Party Match* (on by default) the plugin joins the owner's session and travels there. When the host closes the session, the member gets **On Session Lost**: if **Is In Steam Party** is true, show the party screen instead of the main menu.

<!-- SHOT 41: Blueprint of the owner flow (Create Steam Lobby, ready check, Steam Server Travel, Host Steam Party Session) -->

## Nodes

| Node | What it does |
|---|---|
| **Host Steam Party Session** (also *With Settings*, *From Profile*) | Owner only. Creates a session and begins the party match with it |
| **Begin Steam Party Match** | Owner only. Tells the party where the match is: a *Session Lobby*, a *Server Steam Id* (a listen host without a session) or an *Address* (`ip:port` of a dedicated server) |
| **End Steam Party Match** | Owner only. Ends the party match and opens the party again |
| **Follow Steam Party Match** | Joins the running match. Use it when *Auto Follow* is off, or for a friend who joined the party mid-match |
| **Is In Steam Party**, **Get Steam Party Lobby Id** | Is the player in a party? |
| **Is Steam Party In Match**, **Get Steam Party Match** | Is a match running, and where? |
| **Leave Steam Party** | Leaves the party (a running session stays) |
| **Get Joinable Steam Lobby Id** | The lobby friends should join (the party, else the session). Use it for *Join Game* |

Events on the **Steam Sessions Subsystem**: **On Party Match Started**, **On Party Match Ended**, **On Party Join Finished** (a join from an invite or *Join Game*).

Every lobby node (chat, ready, lobby data, members, kick...) has an advanced **Target** pin: *Active* (the party when there is one, else the session lobby), *Party* or *Session*. The lobby events have a **Source** pin that says which lobby they came from.

## Invites and Join Game

- **Show Steam Invite Overlay** and **Send Steam Session Invite** invite to the party when there is one.
- A friend who accepts joins the **party**, not a session. If that friend is already in another party, the *Join While In Match* setting decides (leave and join, ask the game, or ignore).
- For *Join Game* on the friends list, set the rich presence connect string (Presence nodes) to `+connect_lobby ` followed by **Get Joinable Steam Lobby Id**, after creating or joining the party.

## Settings

**Project Settings > Plugins > Sandwich Steam - Sessions > Party**

| Setting | Default | What it does |
|---|---|---|
| Party Match Visibility | Private | Lobby type of the party during a match. Private hides it from search and friends, invites still work |
| Close Party During Match | off | Nobody can join the party during a match, not even by invite |
| Auto Follow Party Match | on | Members join the owner's match by themselves |
| Auto End Party Match | on | The party match ends when the host's session ends |

## Kick

**Kick Steam Lobby Member** with *Target* **Party** removes a player from the party (their running match is not touched). With *Target* **Session** the player is removed from the match. Call it twice to do both.

## Upgrading from an older version

The lobby events (*On Lobby Chat Received*, *On Lobby Member Changed*, ...) got a new **Source** pin. Right-click each bound event node and pick **Refresh Node** once. Everything else keeps working.

## Good to know

- Lobby data keys starting with `OST` (like `OSTMATCH`) belong to the plugin and are refused by **Set Steam Lobby Data**.
- Call **Host Steam Party Session** when the host already listens (after the travel). Members join right away and would otherwise find nobody to connect to.
- A dedicated server address is only passed on to the members. The plugin does not start servers.
