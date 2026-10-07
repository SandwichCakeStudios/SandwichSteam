<div align="center">

<!-- SHOT 40: Banner: plugin name + a Steam achievement toast over a game scene -->
![Sandwich Steam banner](docs/assets/img/readme/40-hero.png)

# Sandwich Steam

**Steam for Unreal Engine 5, made friendly.**
Achievements, stats, leaderboards, lobbies, voice chat, Steam Input, cloud saves and more.
Use it from **Blueprints** or **C++**. Set it up from an editor dashboard instead of hand-editing config files.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Unreal Engine 5.6 - 5.8](https://img.shields.io/badge/Unreal%20Engine-5.6%20--%205.8-black)
![Platform: Windows tested](https://img.shields.io/badge/Windows-tested-brightgreen)
![Mac / Linux: experimental](https://img.shields.io/badge/Mac%20%2F%20Linux-experimental-orange)

[**Documentation**](https://sandwichcakestudios.github.io/SandwichSteam/) ·
[Installation](https://sandwichcakestudios.github.io/SandwichSteam/installation.html) ·
[Quick Start](https://sandwichcakestudios.github.io/SandwichSteam/quick-start.html) ·
[Publishing](https://sandwichcakestudios.github.io/SandwichSteam/publishing.html) ·
[Troubleshooting](https://sandwichcakestudios.github.io/SandwichSteam/troubleshooting.html)

</div>

---

## Why Sandwich Steam?

| | |
|---|---|
| **Blueprint first** | Every feature has Blueprint nodes with tooltips. No C++ needed to *use* the plugin. |
| **Setup dashboard** | One editor window shows what is missing (App ID, config, data asset) and fixes it with a button. |
| **No Steamworks SDK download** | It uses the Steamworks that ships with Unreal Engine ([see versions](#supported-engine-versions)). |
| **Pick only what you need** | Each feature is its own module. Delete the folder of a feature you do not use. |
| **Tags, not magic strings** | Achievements, stats and leaderboards are chosen from a Gameplay Tag dropdown. Raw Steam names work too. |
| **Built-in test panel** | Press one console command in a running game to see live Steam data and try every feature. |

<!-- SHOT 41: The Steam Dashboard window (Tools > Steam Dashboard) with the status column visible -->
![The Steam Dashboard](docs/assets/img/readme/41-dashboard.png)

## What is inside

| Feature | What you can do |
|---|---|
| **User** | Steam name, Steam ID, avatars, level, ownership and Family Sharing |
| **Utility** | App ID, country, language, server time, Steam Deck / Big Picture detection |
| **Overlay** | Open any Steam overlay page, react to the overlay opening and closing |
| **Stats** | Read, set and add Steam stats. Uploads are batched for you |
| **Achievements** | Unlock, progress toasts, icons, global unlock percentages |
| **Leaderboards** | Upload scores, top N, around me, friends |
| **Friends** | Friend lists, groups, change events |
| **Rich Presence** | Show what the player is doing in the Steam friends list, with translations |
| **Cloud Saves** | Save slots with corruption check and conflict policy |
| **DLC** | Ownership, install, progress, open the store page |
| **Screenshots** | Steam screenshots, viewport capture, location and tagged users |
| **Sessions & Lobbies** | Create, find, join and invite, join from a Steam invite even on a cold start |
| **Voice Chat** | Push to talk or open mic, mute, Steam block list, "who is talking" events |
| **Steam Input** | Controllers as Enhanced Input keys, action sets, glyphs, haptics |
| **Publish Tool** | Download SteamCMD, package your game and upload a build from the editor ([guide](https://sandwichcakestudios.github.io/SandwichSteam/publishing.html)) |

<!-- SHOT 42: A Blueprint graph with 3-4 Sandwich Steam nodes connected (e.g. Set Steam Stat Int -> Unlock Steam Achievement), tooltip visible -->
![Sandwich Steam Blueprint nodes](docs/assets/img/readme/42-blueprint-nodes.png)

## Quick start in 60 seconds

> Sandwich Steam is a **source plugin**: Unreal builds it on your machine the first time. The [installation guide](https://sandwichcakestudios.github.io/SandwichSteam/installation.html) walks you through it, including the one-time Visual Studio setup for Blueprint-only projects.

1. Download this repository (**Code > Download ZIP**) and copy the folder into `YourProject/Plugins/SandwichSteam`.
2. Open your project. When Unreal asks to rebuild the missing modules, click **Yes**.
3. Open **Tools > Steam Dashboard** (or the Steam button in the toolbar) and follow the green checks.
4. Press **Play > Standalone Game** (Steam does not run in Play In Editor) and type `Steam.Test.Toggle` in the console.

Full walkthrough: **[Quick Start: your first achievement in 10 minutes](https://sandwichcakestudios.github.io/SandwichSteam/quick-start.html)**

## Requirements

- Unreal Engine **5.6, 5.7 or 5.8** (see [Supported engine versions](#supported-engine-versions))
- A C++ compiler for the one-time plugin build (Visual Studio with the *Game development with C++* workload on Windows)
- The **Steam client**, running and logged in, when you test
- Nothing to buy: you can develop against Valve's public test app **Spacewar (App ID 480)**

## Supported engine versions

Sandwich Steam uses the Steamworks SDK that ships with Unreal Engine, so the Steamworks version depends on your engine version.

| Unreal Engine | Default Steamworks SDK |
|---|---|
| 5.8 | 1.64 (`Steamv164`) |
| 5.7 | 1.61 (`Steamv161`) |
| 5.6 | 1.57 (`Steamv157`) |

Need a newer Steamworks version than your engine ships with? Epic explains how to download and add it in [Online Subsystem Steam: Downloading Steamworks](https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-steam-interface-in-unreal-engine#downloadingsteamworks).

## Project status

Sandwich Steam is young. This table is honest about what has been tested.

| Area | Status |
|---|---|
| Windows (Win64) | Tested |
| Unreal Engine 5.6, 5.7 | Compiles, Steam Dashboard checked. Full feature testing is done on 5.8 |
| macOS, Linux | **Experimental**: written for them, never run |
| User, Utility, Overlay, Stats, Achievements, Leaderboards | Tested in a Standalone game |
| Friends, Rich Presence, Cloud, Screenshots | Tested with one account (presence data checked on a second one) |
| Sessions & Lobbies | Create, host travel, join and SteamSockets travel tested on two machines. Invites, kick, owner migration and lobby chat **not yet tested** |
| Voice Chat | Starts and lists players. Audio, mute and blocked players **not yet tested** |
| Steam Input | Builds and unit tests pass. **Not yet tested with a real controller** |
| DLC | Works with the test app. **Not yet tested with a real DLC App ID** |
| Editor Dashboard, App Definition, SteamCMD download | Tested on Windows |
| Publish Tool (packaging, real upload, Cancel) | Tested on Windows with a real App ID |
| Dashboard Setup page, confirm window | Tested on Windows |

Found a problem? Please [open an issue](https://github.com/SandwichCakeStudios/SandwichSteam/issues/new/choose).

## Contributing

Bug reports, doc fixes and pull requests are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE) © 2026 Sandwich Cake Studios.

*Sandwich Steam is an independent project. It is not affiliated with or endorsed by Valve Corporation or Epic Games. Steam and the Steam logo are trademarks of Valve Corporation. Unreal Engine is a trademark of Epic Games, Inc.*
