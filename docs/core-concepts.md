---
title: Core Concepts
nav_order: 4
---

# Core Concepts
{: .no_toc }

A few ideas come back in every feature. Read this page once and every feature guide becomes shorter.

1. TOC
{:toc}

## Steam runs in Standalone only

Steam lives in a real game process. In **Play In Editor** every Steam node reports *Unavailable*. Test with **Play > Standalone Game** or a packaged Development build, with the Steam client running and logged in.

Steam belongs to the whole process. When you start several PIE clients, only the first game instance owns Steam; the others stay *Unavailable*.

## The App Definition

The **Steam App Definition** is one data asset that lists what your game has on Steam. Each list belongs to a feature:

| List | Used by |
|---|---|
| Stats | Stats |
| Achievements | Achievements |
| Leaderboards | Leaderboards |
| Presence | Rich Presence |
| DLC | DLC |
| Sessions | Sessions (session profiles) |
| Input Sets | Steam Input |

- **Create it** on the dashboard's **Setup** or **App Definition** page with **Create & assign**, or in the Content Browser under **Add > Steam**.
- **Assign it** in **Project Settings > Plugins > Sandwich Steam > App Definition**. One per project.
- **It is cooked for you.** The editor adds it to the packaging list, so packaged builds find it.
- **Fill it** by hand, or import the achievements with **Import from Steam...** (see the [Quick Start](quick-start.html#import-the-achievements-from-steam)).

The dashboard's status card checks the asset and offers a fix when something is wrong.

## Tags or names

Most nodes pick their target from a **Gameplay Tag** dropdown, for example `Steam.Achievement.FirstWin`. The tag points to a row in the App Definition, and the row holds Steam's own name.

| Tag root | Rows in |
|---|---|
| `Steam.Stat` | Stats |
| `Steam.Achievement` | Achievements |
| `Steam.Leaderboard` | Leaderboards |
| `Steam.Presence` | Presence |
| `Steam.DLC` | DLC |
| `Steam.Session` | Sessions |
| `Steam.Input.ActionSet` | Input Sets |

Add your tags under these roots in **Project Settings > Gameplay Tags**, then pick them in the row's **Tag** field.

Every tag node also has a twin that takes Steam's own name instead: **... By Name** (or **By App ID**, **By Token**). It needs no App Definition row, which is handy for a quick test.

| | Tag node | By Name node |
|---|---|---|
| Pick from a dropdown | Yes | No, you type the name |
| Survives a rename in Steamworks | Yes, change the row once | No, change every node |
| Extras from the row (clamping, progress toasts) | Yes | No |
| Needs an App Definition row | Yes | No |

**Use tags** in your game. A wrong name in a *By Name* node fails and logs one warning.

## Steam state

The plugin waits for Steam before any feature works. The **Steam Core Subsystem** tells you where it is:

| State | Meaning |
|---|---|
| Unavailable | No Steam: Play In Editor, the Steam client is closed, or the platform has no Steam |
| Waiting For Steam | Steam is starting, the user is not logged on yet |
| Ready | Steam works. Every feature is active |
| Offline | The connection to the Steam servers was lost. Features pause until it returns |
| Shutting Down | Steam closed or the game is ending |

Blueprint: get the **Steam Core Subsystem**, then **Is Steam Ready**, **Get Steam State** and the **On Steam State Changed** event. Each feature subsystem also has **Is Feature Active** and **On Feature Active Changed**.

You rarely need these. Nodes that wait for Steam data (such as **Wait For Steam Stats**) do it for you, and a node called too early simply fails with a clear reason.

## Results and errors

Most nodes return a **Steam Result**. Break it to read:

| Field | Meaning |
|---|---|
| Success | True when it worked |
| Error Tag | A `Steam.Error.*` tag that says why it failed. Branch on this in your game logic |
| Message | A readable sentence for logs or a debug UI |
| Native Code | Steam's own error code, for bug reports |

The error tags you will meet most:

| Error tag | Usually means |
|---|---|
| `Steam.Error.Unavailable` | No Steam (Play In Editor, client closed) |
| `Steam.Error.NotInitialized` | Steam or the feature is not ready yet |
| `Steam.Error.Offline` | The Steam servers are not reachable |
| `Steam.Error.FeatureDisabled` | The feature is turned off in the settings |
| `Steam.Error.InvalidArgument` | A missing App Definition row, a wrong name or a wrong type |
| `Steam.Error.Timeout` | Steam did not answer in time |
| `Steam.Error.Cancelled` | The feature stopped while the request was running |
| `Steam.Error.Failed` | Steam refused the call |

Some features add their own, for example `Steam.Error.Lobby.Full` or `Steam.Error.Session.HostLeft`. Their guides list them.

## Async nodes

Nodes that ask Steam over the network are **async nodes** (the ones with a clock icon). They have an **On Success** pin and an **On Failure** pin with a Steam Result. Nodes that return data have one more success pin that carries it, for example **On Entries Loaded** with the leaderboard entries.

- A node finishes once: on success, or on **On Failure**, never both.
- A request that gets no answer fails with `Steam.Error.Timeout` (30 seconds by default).
- When Steam goes offline or the feature stops during the request, it fails with `Steam.Error.Cancelled`.

## Turning features off

**Project Settings > Plugins > Sandwich Steam > Features** has one checkbox per installed feature. An unchecked feature is not created at runtime and costs nothing. Restart the game or editor session after changing it.

To leave a feature out of your game completely, delete its module folder (`Source/SandwichSteam<Feature>`) and its entry in `SandwichSteam.uplugin`. Each feature is its own module, so the others keep working. The feature guides say when one feature needs another (Achievements needs Stats).

## Debug tools

In Standalone or a Development build, open the console with **~**:

| Command | What it does |
|---|---|
| `Steam.Debug.Show` | Live debug panel with a section and buttons per installed feature |
| `Steam.Test.Toggle` | Test panel with one page per feature |
| `Steam.Core.Dump` | Steam state, App ID and readiness. **Include it in bug reports** |
| `Steam.<Feature>.Dump` | The state of one feature, for example `Steam.Stats.Dump` |

None of this exists in Shipping builds.

## Steam IDs

Players and lobbies are identified by a **Steam Id**. **Get Local Steam Id** returns your own. **To String (Steam Id)** and **Make Steam Id From String** convert to and from text (SteamID64 or SteamID3, like `[U:1:22202]`).
