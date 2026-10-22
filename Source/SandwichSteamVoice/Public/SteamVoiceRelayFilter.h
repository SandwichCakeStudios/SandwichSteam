// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamVoiceRules.h"

/** One packet the relay host is about to forward. Views only: copy what you keep. */
struct FSteamVoiceRelayPacket
{
	/** The talker's Steam ID (the transport's sender, never the packet). */
	uint64 Talker = 0;
	uint16 Sequence = 0;
	/** When the host received it (FPlatformTime::Seconds). */
	double ReceiveTime = 0.0;
	/** What the talker sent, before the server rules. */
	bool bOwnerVoice = false;
	TConstArrayView<int32> Channels;
	/**
	 * Steam compressed voice (the talker's GetVoice output). Copy it to record or analyse it elsewhere. Never pass it to DecompressVoice in the
	 * host process: Steam keeps one decoder state per speaker, and the host's own playback of that talker would break.
	 */
	TConstArrayView<uint8> Payload;
};

/**
 * Server Relay moderation hook (C++ only). Set it on the server with USteamVoiceSubsystem::SetRelayFilter; only the relay host calls it
 * (peer to peer never shows packets to the server).
 *
 * Both functions run on the voice worker thread for every forwarded packet: never touch a UObject, never block, keep them to microseconds,
 * and hand heavy work (recording, speech analysis) to your own thread. A filter can only remove: whatever it adds to the route is dropped
 * again, so it can never get around the server rules. Override only what you need; both let everything through by default.
 */
class ISteamVoiceRelayFilter
{
public:
	virtual ~ISteamVoiceRelayFilter() = default;

	/**
	 * Once per packet, after the talker's server rules. InOutRoute is what the rules allow (owner voice, channel ids); remove from it to
	 * restrict this packet for everyone. Return false to drop the packet for everyone.
	 */
	virtual bool FilterPacket(const FSteamVoiceRelayPacket& Packet, SandwichSteam::Voice::FVoiceRoute& InOutRoute)
	{
		return true;
	}

	/**
	 * Once per recipient, after the recipient's server rules (listen channels, owner voice distance). Remove from InOutRoute to restrict what
	 * Recipient gets (pair rules such as "A may not hear B"); return false to send them nothing.
	 */
	virtual bool FilterRecipient(const FSteamVoiceRelayPacket& Packet, uint64 Recipient, SandwichSteam::Voice::FVoiceRoute& InOutRoute)
	{
		return true;
	}
};
