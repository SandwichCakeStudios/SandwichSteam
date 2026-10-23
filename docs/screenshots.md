---
title: Screenshots
parent: Content & Saves
nav_order: 3
---

# Screenshots
{: .no_toc }

Put screenshots into the player's Steam screenshot library, with the place they were taken and the friends in the picture.

> **Status:** tested with one account.

1. TOC
{:toc}

## Before you begin

- Test in **Standalone** or a packaged build with the Steam client running. Steam only keeps screenshots of a game it knows is running.
- No App Definition rows are needed.

## Take a screenshot

| Node | What it does |
|---|---|
| **Trigger Steam Screenshot** | Steam captures the screen, just like the player's screenshot key (F12) |
| **Capture Viewport To Steam** | The game captures its own view and gives it to Steam. Without the Steam overlay and other windows on top |

Use them for a photo mode button, or at a big moment (a win, a record).

<!-- SHOT 74: Blueprint: photo mode "Take photo" button -> Set Steam Screenshot Location ("Harbor") -> Capture Viewport To Steam -->

One viewport capture runs at a time; a second one meanwhile fails with `Steam.Error.RateLimited`.

## Location and tagged players

| Node | What it does |
|---|---|
| **Set Steam Screenshot Location** | Text shown with the screenshot, for example the map name. Stays until you change it; empty clears it |
| **Tag Steam Screenshot User** | Tags a player (Steam ID) in the next screenshot, for example everyone in the match. Up to 32 |

Both also apply to screenshots the player takes with the Steam key.

## Take over the screenshot key

**Project Settings > Plugins > Sandwich Steam - Screenshots > Hook Screenshots** (off by default): when on, the player's screenshot key makes the game capture its own view, like **Capture Viewport To Steam**, instead of Steam capturing the screen. Use it when you want screenshots without the overlay or other windows. **Is Steam Screenshot Hook Active** tells you whether it is on.

**On Screenshot Ready** (*Handle*, *Success*) on the **Steam Screenshots Subsystem** fires when Steam stored a screenshot.

## Console commands

| Command | What it does |
|---|---|
| `Steam.Screenshot [trigger\|capture]` | Takes a screenshot (default: capture) |
| `Steam.Screenshot.Dump` | Hook state, location and counters |

## Good to know

- **Capture Viewport To Steam** needs a game viewport, so it fails with `Steam.Error.NotSupported` on a dedicated server or a commandlet.
- Your UI (UMG) is part of a viewport capture. Hide it first for clean photo mode shots.
- **Client only.**
