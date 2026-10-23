---
title: Stats
parent: Progression
nav_order: 1
---

# Stats
{: .no_toc }

Steam stats are numbers that Steam stores for each player: games played, wins, distance travelled. They survive reinstalls and other PCs, and they drive achievement progress.

> **Status:** tested in a Standalone game.

1. TOC
{:toc}

## Before you begin

- The [Quick Start](quick-start.html) is done: App ID set, an App Definition assigned.
- **The stats exist in Steamworks.** Create them on the [Steamworks partner site](https://partner.steamgames.com/) under *Stats & Achievements* and **publish** the changes. Spacewar (480) already has a few, for example `NumGames` and `NumWins`.

## Add the stats to the App Definition

Steam has no way to list the stats of a game, so you enter them once by hand.

1. Add a tag per stat under `Steam.Stat` in **Project Settings > Gameplay Tags**, for example `Steam.Stat.GamesPlayed`.
2. Open your App Definition and add a row to **Stats** per stat:

| Field | What it does |
|---|---|
| **Api Name** | The stat's API name, exactly as in Steamworks |
| **Tag** | The tag you pick in the nodes |
| **Type** | **Int**, **Float** or **Avg Rate**. Must match Steamworks |
| **Clamp To Range**, **Min**, **Max** | Keep written values inside a range |
| **Default** | The default from Steamworks. Only used to check the row |

Save the asset. The App Definition reports empty or duplicate names and tags, and a *Min* larger than *Max*.

## Read and write stats

Every node picks the stat from a tag dropdown:

| Node | What it does |
|---|---|
| **Get Steam Stat Int**, **Get Steam Stat Float** | Reads the current value |
| **Set Steam Stat Int**, **Set Steam Stat Float** | Sets a new value |
| **Add Steam Stat Int**, **Add Steam Stat Float** | Adds to the current value, for example +1 game played |
| **Update Steam Avg Rate Stat** | For **Avg Rate** stats: *Count* things happened in *Session Seconds* of play |

Use the node that matches the stat's type. A Float node on an Int stat fails with `Steam.Error.InvalidArgument`.

<!-- SHOT 62: Blueprint: match end -> Add Steam Stat Int (Steam.Stat.GamesPlayed, 1) -->

### You do not have to upload

Changes are collected and sent to Steam for you: one upload a few seconds after the first change (setting *Flush Interval Seconds*), and again when the game quits. An achievement unlock uploads at once.

Call **Store Steam Stats Now**, or the async **Store Steam Stats** node to wait for Steam's answer, only when it matters right now, for example right before a level change you know takes long.

### Stats load when the game starts

Steam sends the player's stats shortly after start.

- **Writes before that are kept** and applied when the stats arrive. You can call **Add Steam Stat Int** from **Event BeginPlay**.
- **Reads before that fail** with `Steam.Error.NotInitialized`. Wait with the async node **Wait For Steam Stats**, check **Are Steam Stats Ready**, or bind **On Stats Ready** on the **Steam Stats Subsystem**.

<!-- SHOT 63: Blueprint: Wait For Steam Stats -> On Success -> Get Steam Stat Int -> show in a widget -->

## By name

Each node has a **... By Name** twin that takes the API name instead of a tag, for example **Add Steam Stat Int By Name**. It needs no App Definition row, so it is handy for a quick test, but there is no range clamping and a typo only shows up as a warning in the log. See [Tags or names](core-concepts.html#tags-or-names).

## Settings

**Project Settings > Plugins > Sandwich Steam - Stats**

| Setting | Default | What it does |
|---|---|---|
| Flush Interval Seconds | 5 | How long changes are collected before one upload (1 to 300) |

## Console commands

In Standalone or a Development build:

| Command | What it does |
|---|---|
| `Steam.Stats.Dump` | Ready state, upload state and every stat of the App Definition with its value |
| `Steam.Stats.ResetAll` | Resets **all** stats of your account to their defaults. `Steam.Stats.ResetAll 1` also locks every achievement. Cannot be undone |
| `Steam.Stats.ExportSchema` | Writes the file that **Import from Steam...** reads (achievements only, see [Achievements](achievements.html#import-from-steam)) |

The debug panel (`Steam.Debug.Show`) and the test panel (`Steam.Test.Toggle`, **Stats** page) show every stat live.

## Good to know

- **Stats are client only.** Each player writes their own stats. A dedicated server cannot write them.
- **Steam rejects a value** (wrong type, outside the range set in Steamworks)? The plugin reloads the stats from Steam instead of retrying forever.
- **Offline:** while the Steam servers are unreachable the feature pauses and calls fail with `Steam.Error.Offline`.
- Do not mix these nodes with Unreal's own Online Subsystem achievement or stats nodes for the same data.
