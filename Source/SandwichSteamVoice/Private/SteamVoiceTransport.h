// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Core/SteamSDK.h"

/**
 * Voice transport over ISteamNetworkingMessages: connectionless, through Steam Datagram Relay (no IP addresses are exposed),
 * on its own messaging channel, so voice never shares the game connection's bandwidth.
 *
 * Created and destroyed on the game thread (it registers the session request callback); Send / Receive / Accept / Close are called
 * from the voice worker. The sender of a received message is the transport's peer identity, never anything in the payload.
 * The session request callback runs on Steam's callback thread and only queues the Steam ID for the worker.
 */
class FSteamVoiceTransport
{
public:
	explicit FSteamVoiceTransport(int32 InChannel);
	~FSteamVoiceTransport();

	FSteamVoiceTransport(const FSteamVoiceTransport&) = delete;
	FSteamVoiceTransport& operator=(const FSteamVoiceTransport&) = delete;

	/** SteamNetworkingMessages exists (Steam client ready). */
	bool IsAvailable() const;

	/** Voice data: unreliable, no delay (dropped when it cannot go out at once, like a late syllable should be). */
	bool SendVoice(uint64 Peer, const uint8* Data, int32 Size) const;

	/** Session hello: unreliable but queued until the session is up, so the session exists before the first word. */
	bool SendHello(uint64 Peer, const uint8* Data, int32 Size) const;

	/** Calls Handler for up to MaxMessages received messages and releases them. Returns the number handled. */
	int32 Receive(TFunctionRef<void(uint64 Sender, const uint8* Data, int32 Size)> Handler, int32 MaxMessages = 64) const;

	void AcceptSession(uint64 Peer) const;
	void CloseSession(uint64 Peer) const;

	/** A peer asked to open a session (worker thread). */
	bool PopSessionRequest(uint64& OutPeer) { return SessionRequests.Dequeue(OutPeer); }

	int32 GetChannel() const { return Channel; }

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamVoiceTransport, OnSessionRequest, SteamNetworkingMessagesSessionRequest_t, SessionRequestCallback);

	bool Send(uint64 Peer, const uint8* Data, int32 Size, int32 Flags) const;
#endif

	TQueue<uint64, EQueueMode::Mpsc> SessionRequests;
	int32 Channel = 7;
};
