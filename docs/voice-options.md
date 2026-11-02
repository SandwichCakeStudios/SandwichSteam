---
title: Options Menu & UI
parent: Voice Chat
grand_parent: Multiplayer
nav_order: 4
---

# Options Menu & UI
{: .no_toc }

**For every game, when you build the options menu and the HUD.** Volume, mute, microphone sensitivity, and who is talking.

1. TOC
{:toc}

## Example: a microphone sensitivity slider

Steam has no microphone volume, so **voice activation** is the sensitivity: quieter sound is not sent.

1. When the options menu opens: **Start Steam Voice Mic Test**. The player hears themselves as others would.
2. Every frame: show **Get Steam Local Mic Level** (0 to 1) as a bar.
3. Draw the slider's value as a line on the same bar. On change: **Set Steam Voice Activation Threshold** (0 = off).
4. When the menu closes: **Stop Steam Voice Mic Test**, and save the value in your settings.

Speech is usually between `0.03` and `0.2`. The player sees at once whether their voice reaches the line.

## Options

| Option | Nodes |
|---|---|
| Voice on / off | **Set Steam Voice Enabled**, **Is Steam Voice Enabled** |
| Push to talk / open mic | **Set Steam Voice Mode**, **Get Steam Voice Mode** |
| Voice volume | **Set Steam Voice Master Volume** (0 to 2). Needs **Voice Sound Class** and **Voice Sound Mix** in the Voice settings |
| Volume per player | **Set Steam Player Voice Volume** (Player, 0 to 2). 0 silences them without a mute |
| Microphone sensitivity | **Set Steam Voice Activation Threshold** (see the example above) |
| Hear yourself | **Start Steam Voice Mic Test**, **Stop Steam Voice Mic Test** |

**Is Steam Headset Present** is not reliable: Steam reports a missing microphone as silence. Show the mic level while the player talks instead.

## Mute and blocked players

- **Mute Steam Player** / **Unmute Steam Player**: private, the other player cannot tell. Lasts until the game closes.
- Players you **blocked on Steam** are muted both ways automatically (setting **Auto Mute Blocked Players**).
- **On Player Mute Changed** (Player, Muted, Because Blocked) keeps a player list's mute buttons up to date.

## Talking indicators

The events are on the **Steam Voice Subsystem** (*Get Game Instance Subsystem > Steam Voice Subsystem*, then *Bind Event to ...*).

**Example: a talking icon over a pawn.** Bind **On Player Talking Changed** once. When it fires, find the pawn whose Player State has that *Player* Steam ID, and show or hide its icon with *Talking*.

| Node | What it gives you |
|---|---|
| **On Player Talking Changed** (Player, Talking, Is Local) | A player starts or stops talking. Also for yourself (*Is Local*) |
| **Is Steam Player Talking**, **Get Steam Talking Players** | The same, as getters |
| **Get Steam Player Voice Level** (Player) | 0 to 1, for a level bar. Read it in a UI tick |
| **On Player Voice Flood** (Player) | A player sent far too much voice and was muted for the match. Kick or report them if you want |

A muted player is not shown as talking. To show "muted player is speaking", turn on **Report Muted Talking** (their audio stays silent).

**Next:** [Server Rules & Relay](voice-server.html)
