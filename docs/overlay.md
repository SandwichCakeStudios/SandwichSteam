---
title: Overlay
parent: Social
nav_order: 3
---

# Overlay
{: .no_toc }

Open Steam's overlay pages from your game (friends, achievements, the store, a web page), know when the overlay is open, and pause the game meanwhile.

> **Status:** tested in a Standalone game.

1. TOC
{:toc}

## Before you begin

- The overlay only shows in a **Standalone** game or a packaged build, with the Steam client running. It never shows in Play In Editor.
- In Steam: **Settings > In Game > Enable the Steam Overlay while in-game** must be on.
- Overlay is part of the plugin's core, so it needs no App Definition and is always installed.

## Open a page

**Open Steam Overlay** with a *Dialog*:

| Dialog | Opens |
|---|---|
| **Friends** | The friends list |
| **Community** | The game's community hub |
| **Players** | Players you recently played with |
| **Settings** | Steam settings |
| **Official Game Group** | The game's official group |
| **Stats**, **Achievements** | The player's stats and achievements for this game |
| **Store** | The game's store page |

<!-- SHOT 70: Blueprint: pause menu button "Achievements" -> Open Steam Overlay (Achievements) -->

### Pages about one user

**Open Steam Overlay Target Dialog** with a *Dialog* and a *Target* Steam ID:

| Dialog | Target | Opens |
|---|---|---|
| **User Profile** | A player | Their profile |
| **User Chat** | A player | A chat with them |
| **Add Friend** | A player | The page to send them a friend request |
| **Invite Dialog** | A lobby | Steam's invite window for that lobby |

For invites, **Show Steam Invite Overlay** from [Sessions](sessions.html#invites-and-join-game) is simpler: it picks the right lobby.

### Store and web pages

- **Open Steam Overlay Store** with an *App Id* (0 = this game) and a *Flag*: **None**, **Add To Cart**, or **Add To Cart And Show**. Use it for a "Buy the full game" button or a DLC page.
- **Open Steam Overlay Web Page** with a *Url*. *Modal* on shows only the web page, without the rest of the overlay.

## Is it open?

| Node / event | What it tells you |
|---|---|
| **Is Steam Overlay Enabled** | The overlay can open (Steam running, overlay switched on). Hide your overlay buttons when false |
| **Is Steam Overlay Active** | The overlay is open right now |
| **On Overlay Activated** (*Active*) | On the **Steam Overlay Subsystem**: the overlay opened or closed |

### Pause while it is open

**Project Settings > Plugins > Sandwich Steam > Auto Pause On Overlay** pauses the game while the overlay is open. It only pauses a single-player game, or a host with nobody connected; it never pauses while other players are in the game. A pause your game already had stays after the overlay closes.

For your own behavior (mute music, show a pause menu), bind **On Overlay Activated** instead.

## Console commands

| Command | What it does |
|---|---|
| `Steam.Overlay.Dump` | Enabled and open state |
| `Steam.Overlay.Open <Dialog> [SteamId \| AppId \| Url]` | Opens a page |

## Good to know

- Every open node returns a Steam Result: `Steam.Error.Unavailable` when the overlay cannot open, `Steam.Error.InvalidArgument` for a dialog that belongs to another node (a user page in **Open Steam Overlay**).
- **Mac:** the overlay only works when the game is started from the Steam client. Add your build with *Add a Non-Steam Game to My Library*.
- Overlay not showing? See [Troubleshooting](troubleshooting.html#the-steam-overlay-does-not-show).
- [Friends](friends.html#open-steam-pages) has shortcut nodes for the friends list, profile, chat and add friend.
