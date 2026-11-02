---
title: Voice Chat
parent: Multiplayer
nav_order: 4
has_children: true
---

# Voice Chat
{: .no_toc }

Let players talk to each other over Steam: push to talk or open mic, **proximity voice** from each player's pawn, **radio channels**, per player volume and mute. Voice plays through the **Steam Voice** component, a normal Unreal audio component, so attenuation, submix, sound class and effects work the same way as on any other sound.

> **Status:** tested with two accounts: push to talk, open mic, mute, blocked players, proximity, channels, server rules, Server Relay and lobby voice. Server Relay with three or more players is not tested yet.

1. TOC
{:toc}

## Before you begin

- The [Quick Start](quick-start.html) is done: App ID set, **Configure Steam...** run, an App Definition assigned.
- **Run Configure Steam... again** after installing or updating to this version, then restart the editor. It sets up the voice lines in `DefaultEngine.ini` (see [Upgrading from 1.2](#upgrading-from-12)).
- The **Enhanced Input** plugin is enabled (push to talk uses it).
- Players hear each other once they are in the **same game**: a [session](sessions.html), or any listen server they joined. To talk in a main menu lobby before the match, see [Lobby voice](voice-rules.html#lobby-voice).
- **Test in Standalone Game** or a packaged build, not in Play In Editor. A real test needs **two machines and two Steam accounts**; on one machine use a [fake talker](voice-reference.html#testing-on-one-machine).
- **Only the first local player** has voice. Split screen guests share the one microphone.
- **Dedicated servers are not supported** yet. The host is always a player (listen server).

## How it works in one picture

```
Player A talks
 └─ Steam microphone ─ voice packet ─ Steam network ─> Player B
                                                        ├─ A's pawn:  Steam Voice (Owner)    = A's mouth, 3D
                                                        └─ B's radio: Steam Voice (Channel)  = everyone on that channel
```

- A **talker** is a player whose microphone sends.
- A **speaker** is a **Steam Voice** component. It decides **which** voices it plays; its audio settings decide **how** they sound.
- You need no speakers for basic voice: a pawn without one gets one automatically (2D, heard everywhere).

## Basic voice in 3 steps

1. Create an **Input Action** (`IA_PushToTalk`, Value Type *Digital (bool)*) and an **Input Mapping Context** that maps it to a key, for example **V**.
2. In **Project Settings > Plugins > Sandwich Steam - Voice > Push To Talk**, set **Push To Talk Action** and **Push To Talk Context**.
3. Host and join a match. Hold **V** to talk.

That is all. The plugin binds the action to the first local player controller, adds a speaker to every talking player's pawn, and plays players without a pawn in 2D.

**Open mic instead?** Set **Default Mode** (General) to **Open Mic**, or call **Set Steam Voice Mode** from your options menu. Use **Start Steam Talking** / **Stop Steam Talking** if you want to drive the microphone yourself (for example from a UI button).

<!-- SHOT 75: Project Settings > Plugins > Sandwich Steam - Voice, Push To Talk section with IA_PushToTalk and IMC_Voice assigned -->

## What next

Each page stands on its own. Read only the ones your game needs.

| I want to... | Go here |
|---|---|
| Hear players **from their pawn**, fading with distance | [Proximity & Speakers](voice-proximity.html) |
| Add a **team radio**, walkie or loudspeaker | [Radio Channels](voice-channels.html) |
| Turn voice **off in menus**, silence **dead players**, or talk in a **main menu lobby** | [When Players Can Talk](voice-rules.html) |
| Build the **options menu** (volume, mute, sensitivity) and **talking icons** | [Options Menu & UI](voice-options.html) |
| Stop players from **overhearing** the other team, or let the host **moderate** voice | [Server Rules & Relay](voice-server.html) |
| Look up a **node, setting or console command** | [Reference](voice-reference.html) |

## Upgrading from 1.2

Version 1.3 plays voice through Steam directly (clearer and louder) instead of Unreal's Online Subsystem voice. **Use Steam Voice Pipeline** is on by default.

1. Update the plugin, open the project, click **Configure Steam...** and apply. It removes the Online Subsystem voice lines from `DefaultEngine.ini`.
2. Restart the editor.

Your voice nodes keep working; nothing to refresh. Pawns get a 2D speaker automatically, so voice sounds as before until you add [proximity](voice-proximity.html).

**Keep the old voice:** untick **Use Steam Voice Pipeline** before you run Configure Steam. The Online Subsystem voice supports push to talk, open mic, mute, blocked players and the master volume. Speakers, channels, per player volume, levels, the mic test, server rules and lobby voice need the Steam pipeline (the nodes return `Steam.Error.NotSupported`).

**Next:** [Proximity & Speakers](voice-proximity.html)
