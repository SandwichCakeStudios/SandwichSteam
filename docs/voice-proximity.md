---
title: Proximity & Speakers
parent: Voice Chat
grand_parent: Multiplayer
nav_order: 1
---

# Proximity & Speakers
{: .no_toc }

**For most games.** Players are heard from their pawn and fade with distance, like any other 3D sound. This page also explains the **Steam Voice** component that plays every voice.

1. TOC
{:toc}

## Example: proximity voice in 3 steps

1. Create a Blueprint class with parent **Steam Voice**, for example `BP_VoiceMouth`.
2. In it, set **Attenuation Settings** (or **Override Attenuation**): for example *Falloff Distance* `1500`, spatialization on.
3. In **Project Settings > Plugins > Sandwich Steam - Voice > Speakers**, set **Voice Component Class** to `BP_VoiceMouth`.

Every pawn that talks now gets a `BP_VoiceMouth`. Prefer to place it yourself? Add `BP_VoiceMouth` to your pawn Blueprint; the plugin then adds nothing.

**Change the range at runtime** with the engine's **Adjust Attenuation** node on the speaker. It applies at once, also while the player talks (for example a "shout" button).

<!-- SHOT 76: Pawn Blueprint with a BP_VoiceMouth component selected, Attenuation settings visible in the Details panel -->

## The Steam Voice component

Add **Steam Voice** (Add Component > Audio) to any actor. Pick its **Source**:

| Source | Plays | Put it on |
|---|---|---|
| **Owner** (default) | The player who owns this pawn: their **mouth** | The player's pawn (or an actor the pawn owns) |
| **Channel** | Everyone talking on its **Channel**, mixed together: radio, walkie, PA speaker | Any actor: a pawn, a radio prop, a loudspeaker in the level. See [Radio Channels](voice-channels.html) |

| Property / node | What it does |
|---|---|
| **Channel** | The channel a *Channel* speaker plays |
| **Include Owner** | Off (default): the radio on A's pawn does not play A, so A is not heard twice |
| **Set Channel** | Switches the channel at runtime (a radio dial) |
| **Activate** / **Deactivate** | Your on / off switch. A switched off Owner speaker keeps that pawn silent |
| **Get Talkers**, **Is Anyone Talking**, **Get Voice Level** | Who this speaker plays right now, and how loud |

Everything else is the usual audio setup of the component: **Attenuation**, **Sound Class**, **Submix** sends, **Source Effects**, **Concurrency**. A speaker only plays while there is voice, so silent speakers cost nothing.

**Automatic speakers:** a talking player's pawn without an Owner speaker gets one of **Voice Component Class** (setting **Auto Add Voice Component**). A player without a pawn plays on a 2D speaker. Speakers you place yourself are never removed by the plugin.

## Dead players

By default a player without a pawn (dead, spectating, in a menu) is heard in 2D everywhere. In a proximity game that means a dead player is heard across the map.

Call **Set Steam Pawnless Voice** with **Silent** when the match starts. The player is silent until they have a pawn again; their talking events still fire, so a talking icon still works. More in [When Players Can Talk](voice-rules.html).

## Give voice a high priority

Voice plays through normal audio components, so it competes with game sounds for the engine's voice limit. In a busy scene the engine could stop a player's voice. Give your speaker class a **high Priority** and a **Sound Concurrency** that never stops voice (or none that limits it).

**Next:** [Radio Channels](voice-channels.html)
