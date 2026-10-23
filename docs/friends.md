---
title: Friends
parent: Social
nav_order: 1
---

# Friends
{: .no_toc }

Read the player's Steam friends list, see who is online or playing your game, react when that changes, and open Steam's chat, profile and add-friend pages from your UI.

> **Status:** tested with one account.

1. TOC
{:toc}

## Before you begin

- The [Installation](installation.html) and the first steps of the [Quick Start](quick-start.html) are done (App ID, Configure Steam). Friends needs **no App Definition** rows.
- Test in **Standalone** with the Steam client running. Friends who are online make the test more interesting.

## Show the friends list

1. **Read Steam Friends** (async) with:
   - **Source**: **Friends**, **Blocked** or **Recent Players** (people you played with).
   - **Filter**: **All**, **Online**, **In Game** (any game) or **In This Game**.
2. **On Friends Read** gives an array of **Steam Friend Info**, sorted online first, then by name.
3. Create one list entry per friend.

<!-- SHOT 65: Blueprint: Read Steam Friends (Friends, Online) -> On Friends Read -> For Each -> Create friend entry widget -->

Each **Steam Friend Info** has:

| Field | Meaning |
|---|---|
| **Steam Id** | Who it is. Use it for avatars, invites and the Open nodes |
| **Name** | The Steam name |
| **Persona State** | **Online**, **Offline**, **Away**, **Busy**, **Snooze**, ... |
| **In Game**, **Game App Id** | Playing any game, and which one |
| **In This Game** | Playing your game right now |
| **Lobby Id** | The lobby the friend is in, when it is a lobby of your game |
| **Info Loaded** | False when Steam has not sent this user's name yet (can happen for recent players) |

A friends list reads instantly: Steam already has it. **Recent Players** who are not friends may need their names from Steam first; the node waits up to 5 seconds for them.

### Avatars

The list does not load pictures, so a list of 300 friends stays cheap. In each entry widget, call **Get Steam Avatar** (async) with the friend's *Steam Id* and a size (**Small** 32x32, **Medium** 64x64, **Large** 184x184). **On Avatar Loaded** gives a texture for an Image widget.

## Single checks

| Node | What it does |
|---|---|
| **Get Steam Friend Count** | How many friends the player has |
| **Is Steam Friend** | Is this Steam ID a friend? For example to mark friends in a scoreboard |
| **Get Steam Friend Info** | The info of one user right now |
| **Get Steam Friend Groups** | The player's friend groups (Steam calls them tags), with their members |

## React to changes

On the **Steam Friends Subsystem**:

| Event | When |
|---|---|
| **On Friends List Changed** | A friend was added, removed or blocked. Read the list again |
| **On Friend State Changed** (*Friend*, *Change Flags*) | A friend changed. Update that one entry instead of rebuilding the list |

*Change Flags* can hold several changes at once. Test them with **Has Steam Friend Change**: **Name**, **State** (came online, went away, ...), **Game** (started a game, joined a lobby), **Avatar** (load it again) or **Relationship**.

<!-- SHOT 66: Blueprint: On Friend State Changed -> Has Steam Friend Change (State) -> Branch -> update the entry's status -->

Steam sends a burst of changes when the player logs on. The plugin merges them, so you get at most one event per friend per frame.

## Open Steam pages

These open the Steam overlay on top of your game:

| Node | Opens |
|---|---|
| **Open Steam Friends List** | The Steam friends list |
| **Open Steam Profile** | A user's Steam profile |
| **Open Steam Chat** | A chat window with a user |
| **Open Steam Add Friend** | The page to send a friend request |

Steam has no way for a game to send or accept friend requests itself; **Open Steam Add Friend** is how a player does it.

To **invite** a friend to a match, use **Send Steam Session Invite** with the friend's *Steam Id* (see [Sessions](sessions.html#invites-and-join-game)).

## Console commands

In Standalone or a Development build:

| Command | What it does |
|---|---|
| `Steam.Friends.Dump` | Counts (online, in a game, in this game), groups, the first 15 friends |
| `Steam.Friends.List [All\|Online\|InGame\|InThisGame] [Friends\|Blocked\|Recent]` | Logs a list |

The debug panel (`Steam.Debug.Show`) has a **Friends** section, and the test panel (`Steam.Test.Toggle`) a **Friends** page.

## Good to know

- **Client only.** A dedicated server has no friends list.
- The **Open** nodes need the Steam overlay. Without it they fail with `Steam.Error.NotSupported`; see [Troubleshooting](troubleshooting.html#the-steam-overlay-does-not-show).
- **Friend groups** depend on the Steamworks version that ships with your engine. When it has none, **Get Steam Friend Groups** returns an empty list.
- The friend's status text in the Steam list ("Playing on Arena") is rich presence, a separate feature.
