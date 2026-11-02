---
title: Server Rules & Relay
parent: Voice Chat
grand_parent: Multiplayer
nav_order: 5
---

# Server Rules & Relay
{: .no_toc }

**Advanced. Only needed when hearing the other team would be cheating, or when the host must control voice** (server mute, dead chat, voice range). Co-op games usually skip this page.

1. TOC
{:toc}

## Why

Without this page, every client receives every player's voice and channels, and only **chooses** what to play. Proximity and channels are not private: a modified client could hear the whole map and every radio.

| Level | Who decides | Safe against |
|---|---|---|
| Nothing (default) | Each client | Nobody. Fine for co-op |
| **Server rules** | The host. Honest clients do not send what is forbidden | Single cheaters |
| **Server rules + Server Relay** | The host forwards only what each player may hear | Cheaters working together |

## Example: dead chat

The living cannot hear the dead; the dead hear each other on a `Dead` channel.

1. Tick **Server Authority** (Voice settings > Server Authority) in every build.
2. When a player dies (on the server):
   - **Server Set Steam Player Can Use Owner Voice** = false.
   - **Server Set Steam Player Channels**: Transmit and Listen both *Restricted* to `Steam.Voice.Channel.Dead`.
3. For living players: **Server Set Steam Player Channels** with Listen *Restricted* to their team channels (without `Dead`).
4. On respawn: Can Use Owner Voice = true, Transmit and Listen not restricted.

Listen rules cover channels only: a dead player still hears living players whose pawns are near their camera.

## Server rules

Tick **Server Authority** (Voice settings) in **every** build that plays together. The host calls these nodes; every client follows. Clients that call them get `Steam.Error.WrongScope`.

| Node (server only) | What it does |
|---|---|
| **Server Set Steam Player Can Talk** | Server mute: nobody hears the player |
| **Server Set Steam Player Can Use Owner Voice** | Off: the player is heard only on channels, not from their pawn |
| **Server Set Steam Player Channels** | Which channels the player may talk on (*Transmit*) and hear (*Listen*). Not *Restricted* = all channels; Restricted with no channels = none |
| **Server Set Steam Owner Voice Max Distance** | Beyond this distance (cm) a player's own voice is not sent at all. 0 = no limit. Keep it at least your attenuation's falloff distance |
| **Server Set Steam Player Owner Voice Max Distance** | One player's own distance: a **megaphone** (below 0 = no limit, 0 = the game's) |
| **Set Steam Voice Allowed**, **Set Steam Pawnless Voice** | The [game rules](voice-rules.html), now for everyone |

- Rules are kept on the host until it closes, also when a player leaves and joins again or the match travels to a new map.
- After joining, a client stays silent for a moment until the rules arrive (**On Voice Rules Changed**, **Is Steam Voice Waiting For Rules**).
- Rules are readable by every client (**Get Steam Player Voice Rules**), for example to show "muted by the host".

## Server Relay

Server Relay sends all voice through the host, which forwards only what each player may hear. Clients never exchange voice directly.

```
Peer to peer:   A ──> B     A ──> C     B ──> C   (every client gets everything)
Server Relay:   A ──> Host ──> B, C               (each gets only what the rules allow)
```

**Turn it on:** with Server Authority on, set **Topology** (Voice settings > Server Authority) to **Server Relay**, or call **Server Set Steam Voice Topology** on the host.

| | Peer To Peer (default) | Server Relay |
|---|---|---|
| Hidden channels and distance | Respected by honest clients | Enforced by the host |
| Latency | Direct | One extra hop |
| Host upload | Its own voice | Every forwarded stream (8 players, 3 talking: about 63 KB/s) |

- Needs a **listen server** (the host is a player).
- **Blocked on Steam:** the host does not know your block list and still forwards to the player you blocked. Your own block and mute always work for what you hear.

## Moderation (C++)

The relay host can inspect or drop every forwarded packet with a filter. Example: silence players on the game's own blocklist for everyone.

```cpp
#include "SteamVoiceRelayFilter.h"
#include "SteamVoiceSubsystem.h"

class FMyVoiceFilter : public ISteamVoiceRelayFilter
{
public:
	TSet<uint64> Silenced; // fill it before SetRelayFilter, or guard it with a lock

	virtual bool FilterPacket(const FSteamVoiceRelayPacket& Packet, SandwichSteam::Voice::FVoiceRoute& InOutRoute) override
	{
		return !Silenced.Contains(Packet.Talker);
	}
};

// On the server, for example in your Game Mode's BeginPlay:
if (USteamVoiceSubsystem* Voice = GetGameInstance()->GetSubsystem<USteamVoiceSubsystem>())
{
	Voice->SetRelayFilter(MakeShared<FMyVoiceFilter, ESPMode::ThreadSafe>());
}
```

- The filter runs on the **voice thread**: never touch a UObject and never block in it.
- It can only remove voice, never add it, so it cannot get around the rules.
- Override **FilterRecipient** for pair rules such as "A may not hear B".
- In Blueprint, **On Server Voice Relay Changed** tells the host who is talking on what (on changes only).

**Next:** [Reference](voice-reference.html)
