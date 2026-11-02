---
title: Radio Channels
parent: Voice Chat
grand_parent: Multiplayer
nav_order: 2
---

# Radio Channels
{: .no_toc }

**For team radios, walkie-talkies and loudspeakers.** A channel is a group of listeners: everyone who talks on it is heard on every *Channel* speaker set to it, anywhere on the map.

1. TOC
{:toc}

## Example: a team radio

**1. Make the channel.** Add a tag under `Steam.Voice.Channel` in **Project Settings > Gameplay Tags**, for example `Steam.Voice.Channel.RadioBlue`. Then add it to **Channel Tag Map** (**Project Settings > Plugins > Sandwich Steam - Voice > Channels**) with an id, for example `1`.

**2. Give every pawn two speakers:**

| Component | Source | Settings |
|---|---|---|
| `Voice_Mouth` | Owner | Attenuation (see [proximity](voice-proximity.html)) |
| `Voice_Radio` | Channel `Steam.Voice.Channel.RadioBlue` | 2D, a radio source effect, Include Owner off |

**3. Bind a Radio key** in your pawn or controller:

- **Pressed:** **Add Steam Voice Transmit Channel** (`RadioBlue`), then **Start Steam Talking**.
- **Released:** **Remove Steam Voice Transmit Channel** (`RadioBlue`), then **Stop Steam Talking**.

**Result:** player A holds Radio. B hears A from A's pawn when close, and from B's own radio anywhere on the map. Two players on the radio at once are mixed.

<!-- SHOT 77: Blueprint: Radio key Pressed -> Add Steam Voice Transmit Channel (RadioBlue) -> Start Steam Talking; Released -> Remove ... -> Stop Steam Talking -->

## Variations

| I want... | Do this |
|---|---|
| **Radio only**, not heard from the pawn | **Set Steam Voice Transmit To Owner** = false |
| A **"Blue is on the radio" icon** | Bind **On Player Transmit Channels Changed**, or ask **Is Steam Player Transmitting On** (Player, `RadioBlue`) |
| A **radio dial** | **Set Channel** on the radio speaker |
| A **loudspeaker** in the level | A Steam Voice component with Source *Channel* on any actor, with attenuation |
| **Who is on** this radio | **Get Talkers** on the radio speaker |
| **Several channels** at once | **Set Steam Voice Transmit Channels** with a list (up to 8) |

## One channel map for everyone

- Every build that plays together needs the **same Channel Tag Map**. A client with a different map hears the wrong channels.
- Two tags with the same id are the same channel.
- A tag that is not in the map cannot be used: transmitting on it fails, and speakers set to it stay silent. **Validate Project** lists tags you forgot to map.
- Nodes take a **Steam Voice Channel**: a tag, or a plain id (**Make Steam Voice Channel From Tag / From Id**). The tag wins when both are set.

Channels are not private by themselves: every client receives them and only chooses what to play. To keep a team's radio from the other team, see [Server Rules & Relay](voice-server.html).

**Next:** [When Players Can Talk](voice-rules.html)
