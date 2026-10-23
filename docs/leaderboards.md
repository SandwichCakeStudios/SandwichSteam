---
title: Leaderboards
parent: Progression
nav_order: 3
---

# Leaderboards
{: .no_toc }

Upload scores and show the best players, the players around you, or only your friends.

> **Status:** tested in a Standalone game.

1. TOC
{:toc}

## Before you begin

- The [Quick Start](quick-start.html) is done: App ID set, an App Definition assigned.
- **The leaderboard exists in Steamworks**, or you let the game create it (*Create If Missing* below). Steam has no way to list a game's leaderboards, so you enter them by hand.
- **On Spacewar (480)** use a row with *Create If Missing* on, for example a board named `SandwichSteamTest`.

## Add the leaderboards to the App Definition

1. Add a tag per leaderboard under `Steam.Leaderboard`, for example `Steam.Leaderboard.HighScore`.
2. Add a row to **Leaderboards** in your App Definition:

| Field | What it does |
|---|---|
| **Name** | The leaderboard's name, exactly as in Steamworks |
| **Tag** | The tag you pick in the nodes |
| **Create If Missing** | Create the board when it does not exist yet. Only for boards not managed in Steamworks |
| **Sort Method** | **Descending** (high scores win) or **Ascending** (low times win). Used when the game creates the board |
| **Display Type** | **Numeric**, **Time Seconds** or **Time Milli Seconds**. Used when the game creates the board |

## Upload a score

**Upload Steam Leaderboard Score** (async) with:

- **Leaderboard Tag**, **Score**.
- **Method**: **Keep Best** (only a better score is stored, the usual choice) or **Force Update** (always store it).
- **Details** (optional): up to 64 extra numbers stored with the score, for example the character or the lap times.

**On Score Uploaded** tells you what happened: *Score*, *Score Changed* (false when an older score was better), *New Rank* and *Previous Rank* (0 = first entry).

<!-- SHOT 67: Blueprint: level complete -> Upload Steam Leaderboard Score (Steam.Leaderboard.HighScore, Keep Best) -> On Score Uploaded -> "New rank: X" -->

## Show the leaderboard

1. Build a query with one of these nodes:

| Node | Returns |
|---|---|
| **Make Steam Leaderboard Top Query** (*Count*, default 10) | The best players |
| **Make Steam Leaderboard Around User Query** (*Before* 4, *After* 5) | The player and the ranks around them |
| **Make Steam Leaderboard Friends Query** | Only the player's friends |
| **Make Steam Leaderboard Users Query** (*Users*) | Only these players, for example everyone in this match |

2. **Download Steam Leaderboard Entries** (async) with the tag and the query.
3. **On Entries Loaded** gives an array of entries: *Steam Id*, *Rank*, *Score*, *Details*.

<!-- SHOT 68: Blueprint: Make Steam Leaderboard Top Query (10) -> Download Steam Leaderboard Entries -> On Entries Loaded -> For Each -> row widget -->

The entries carry no names or pictures, so a download stays fast. In each row widget, use the entry's *Steam Id*:

- **Get Steam Avatar** (async) loads the picture, for any player (see [Friends](friends.html#avatars)).
- **Get Steam Friend Info** gives the *Name*, also for players who are not friends. When *Info Loaded* is false, Steam is still fetching that player: read it again once **Get Steam Avatar** finished, or a moment later.

One download returns at most 100 entries. For the next page, fill a **Steam Leaderboard Query** yourself: *Type* **Global**, *Range Start* 101, *Range End* 200.

## By name

**Upload Steam Leaderboard Score By Name** and **Download Steam Leaderboard Entries By Name** take the leaderboard's name instead of a tag. They need no App Definition row, but the board must already exist (*Create If Missing* comes from the row). See [Tags or names](core-concepts.html#tags-or-names).

## Console commands

In Standalone or a Development build:

| Command | What it does |
|---|---|
| `Steam.Leaderboards.Dump` | The rows, cached boards and download counters |
| `Steam.Leaderboards.Upload <Name> <Score>` | Uploads a score (keep best) and logs the new rank |
| `Steam.Leaderboards.Top <Name> [Count]` | Logs the best entries |

The test panel (`Steam.Test.Toggle`) has a **Leaderboards** page.

## Good to know

- **Trusted leaderboards** (marked "trusted" in Steamworks) do not accept scores from the game. The upload fails with `Steam.Error.NotSupported`.
- **Identical downloads share one request.** Two widgets asking for the same Top 10 at the same time cost one Steam call.
- A download that gets no answer fails after 30 seconds with `Steam.Error.Timeout`.
- **Client only.** A dedicated server cannot upload scores for players.
- Do not mix these nodes with Unreal's own Online Subsystem leaderboard nodes for the same boards.
