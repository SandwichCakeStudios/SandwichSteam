---
title: Reference
parent: Voice Chat
grand_parent: Multiplayer
nav_order: 6
---

# Voice Chat Reference
{: .no_toc }

**For lookup.** Every node, event, setting and console command of Voice Chat in one place. The other pages explain how to use them.

1. TOC
{:toc}

## Nodes

All in the **Steam | Voice** category.

| Group | Nodes |
|---|---|
| Player options | **Set / Is Steam Voice Enabled**, **Set / Get Steam Voice Mode**, **Set / Get Steam Voice Master Volume**, **Set / Get Steam Player Voice Volume** |
| Microphone | **Start / Stop Steam Talking**, **Is Steam Local Player Talking**, **Bind Steam Push To Talk**, **Is Steam Push To Talk Bound**, **Set / Get Steam Voice Activation Threshold**, **Get Steam Local Mic Level**, **Start / Stop Steam Voice Mic Test**, **Is Steam Voice Mic Testing**, **Is Steam Headset Present** |
| Game rules | **Set / Is Steam Voice Allowed**, **Set / Get Steam Pawnless Voice**, **Is Steam Lobby Voice Active** |
| Channels (talking) | **Set / Add / Remove / Clear Steam Voice Transmit Channel(s)**, **Get Steam Voice Transmit Channels**, **Is Steam Voice Transmitting On**, **Set Steam Voice Transmit To Owner**, **Is Steam Voice Transmitting To Owner**, **Make Steam Voice Channel From Tag / From Id**, **Resolve Steam Voice Channel Id** |
| Other players | **Is Steam Player Talking**, **Get Steam Talking Players**, **Get Steam Player Voice Level**, **Get Steam Player Transmit Channels**, **Is Steam Player Transmitting On**, **Get Steam Player Voice Components** |
| Mute | **Mute / Unmute Steam Player**, **Is Steam Player Muted**, **Get Steam Muted Players** |
| Server rules | **Server Set Steam Player Can Talk**, **Server Set Steam Player Can Use Owner Voice**, **Server Set Steam Player Channels**, **Server Set Steam Owner Voice Max Distance**, **Server Set Steam Player Owner Voice Max Distance**, **Server Set Steam Voice Topology**, **Get Steam Player Voice Rules**, **Get Steam Owner Voice Max Distance**, **Get Steam Voice Topology**, **Has Steam Voice Authority**, **Is Steam Voice Relay Host**, **Is Steam Voice Waiting For Rules** |
| Steam Voice component | **Set Channel**, **Get Talkers**, **Is Anyone Talking**, **Get Voice Level**, **Is Speaker On** |

**Events** on the **Steam Voice Subsystem**: **On Player Talking Changed**, **On Voice Mode Changed**, **On Voice Allowed Changed**, **On Player Mute Changed**, **On Player Transmit Channels Changed**, **On Player Voice Flood**, **On Voice Rules Changed**, **On Voice Identity Conflict** (server: two players with one Steam ID), **On Server Voice Relay Changed** (relay host).

**Errors:** actions return a result with an *Error Tag* (see [Core Concepts](core-concepts.html)). The common ones:

| Error | Means |
|---|---|
| `Steam.Error.WrongScope` | A server node was called on a client |
| `Steam.Error.NotSupported` | The node needs the Steam voice pipeline, or the master volume has no sound class and mix |
| `Steam.Error.InvalidArgument` | A bad Steam ID, an unmapped channel tag, or more than 8 channels |

## Settings

**Project Settings > Plugins > Sandwich Steam - Voice**

| Section | Setting | Default | What it does |
|---|---|---|---|
| General | Enabled By Default | on | The player's voice option at start |
| | Voice Allowed By Default | on | The game's rule at start (**Set Steam Voice Allowed**) |
| | Allow Voice In Lobby | off | [Lobby voice](voice-rules.html#lobby-voice) |
| | Use Steam Voice Pipeline | on | Off = the older Online Subsystem voice (see [Upgrading from 1.2](voice.html#upgrading-from-12)) |
| | Default Mode | Push To Talk | Or Open Mic |
| | Auto Mute Blocked Players | on | Players blocked on Steam are muted both ways |
| | Report Muted Talking | off | Muted players still fire talking events (no audio) |
| | Mute Steam Friends Voice While Talking | off | Steam's own friends voice chat goes quiet while the player talks in game |
| Microphone | Voice Activation Threshold | 0 (off) | Microphone sensitivity, 0 to 1 |
| Pipeline | Playout Delay Ms | 200 | Buffer against network jitter. Lower values stutter |
| Channels | Channel Tag Map | empty | Channel tag to id ([Radio Channels](voice-channels.html)) |
| Server Authority | Server Authority | off | [Server rules](voice-server.html#server-rules) |
| | Default Owner Voice Max Distance | 0 (no limit) | Start value of the distance rule |
| | Topology | Peer To Peer | Or [Server Relay](voice-server.html#server-relay) |
| Speakers | Auto Add Voice Component | on | Pawns without a speaker get one |
| | Voice Component Class | Steam Voice (2D) | The class that is added, for example your [proximity](voice-proximity.html) speaker |
| | Default Pawnless Voice | 2D | Start value of **Set Steam Pawnless Voice** |
| Push To Talk | Push To Talk Action, Push To Talk Context | empty | Your Enhanced Input action and mapping context |
| | Auto Bind Push To Talk | on | Bind the action to the first local player controller by itself |
| Volume | Voice Sound Class, Voice Sound Mix | empty | Needed for the master volume. Speakers without a sound class get this one |

## Console commands and test panel

In Standalone or a Development build:

| Command | What it does |
|---|---|
| `Steam.Voice.Dump` | Mode, microphone, who talks, mutes, rules, and per player packets, losses and buffer |
| `Steam.Voice.Talk 1` / `0` | Opens / closes the microphone |
| `Steam.Voice.Mode open` / `ptt` | Open mic or push to talk |
| `Steam.Voice.MicTest 1` / `0` | Hear yourself |
| `Steam.Voice.Mute <SteamID64>` / `Unmute` | Mute a player |
| `Steam.Voice.Threshold 0.05` | Microphone sensitivity |
| `Steam.Voice.Transmit 1 1` | Talk from the pawn (first `1`) and on channel id 1 |
| `Steam.Voice.FakeTalker 1` | One fake player who repeats what you say (see below) |

The test panel (`Steam.Test.Toggle`) has a **Voice** page: level bars per player, mic test, fake talkers, mute, server rules and topology buttons.

## Testing on one machine

Steam has one microphone per PC, so a second game on the same machine cannot talk. Use a **fake talker** instead:

1. `Steam.Voice.Talk 1` (opens the microphone).
2. `Steam.Voice.FakeTalker 1`. Everything you say comes back a moment later as another player, through the full receive path, on its own speaker.
3. `Steam.Voice.FakeTalker 0` removes it.

`Steam.Voice.FakeTalker 1 100 0 250 1` makes it talk on channel 1 only, to test a [radio](voice-channels.html).

**Back to:** [Voice Chat](voice.html)
