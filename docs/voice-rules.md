---
title: When Players Can Talk
parent: Voice Chat
grand_parent: Multiplayer
nav_order: 3
---

# When Players Can Talk
{: .no_toc }

**For games with menus, lobbies, or players who die.** Your game decides when voice runs and how players without a pawn are heard, separate from the player's own options.

1. TOC
{:toc}

## Examples

- **Voice in the match, not in the lobby:** untick **Voice Allowed By Default** (Voice settings > General). At match start: **Set Steam Voice Allowed** = true.
- **2D in the lobby, proximity in the match:** lobby: **Set Steam Pawnless Voice** = 2D. Match start: **Set Steam Pawnless Voice** = Silent.
- **Dead players talk to each other:** a `Steam.Voice.Channel.Dead` [channel](voice-channels.html) and a *Channel* speaker for it on the spectator, with Pawnless Voice Silent. To keep it from the living, see [Dead chat](voice-server.html#example-dead-chat).

## The two switches

| Node | What it does |
|---|---|
| **Set Steam Voice Allowed** | Your game's rule. False: the microphone closes and nobody is heard. The player's own choice (**Set Steam Voice Enabled**) is kept |
| **Set Steam Pawnless Voice** | How a player **without a pawn** (menus, spectating, dead) is heard: **2D** (everywhere) or **Silent** |

**Where to call them:**

- Without [server rules](voice-server.html): **on every client**, for example when a replicated match state changes.
- With server rules: **on the server** only. Every client follows.

**On Voice Allowed Changed** fires when the rule changes, for example to hide the push to talk hint.

## Lobby voice

Players in the same **Steam lobby** hear each other before there is a match, for example in a main menu lobby made with **Create Steam Lobby** or a [party](party-lobby.html).

1. Tick **Allow Voice In Lobby** (Voice settings > General).
2. Create or join a Steam lobby. Voice starts by itself, in 2D.

When the host starts the match (listen server), voice switches to the match's players and pauses for a moment while everyone travels. **Is Steam Lobby Voice Active** tells you when lobby voice runs.

- A lobby **map** that everyone already joined as a listen server needs none of this: it has voice already.
- Mute, volume and **Set Steam Voice Allowed** work in the lobby too. Server rules and Server Relay start with the match.

**Next:** [Options Menu & UI](voice-options.html)
