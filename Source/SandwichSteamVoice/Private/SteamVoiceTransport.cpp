// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceTransport.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

namespace
{
	SteamNetworkingIdentity MakeIdentity(uint64 Peer)
	{
		SteamNetworkingIdentity Identity;
		Identity.SetSteamID64(Peer);
		return Identity;
	}
}

FSteamVoiceTransport::FSteamVoiceTransport(int32 InChannel)
	: Channel(FMath::Max(InChannel, 0))
{
	SessionRequestCallback.Register(this, &FSteamVoiceTransport::OnSessionRequest);
}

FSteamVoiceTransport::~FSteamVoiceTransport()
{
	SessionRequestCallback.Unregister();
}

bool FSteamVoiceTransport::IsAvailable() const
{
	return SteamNetworkingMessages() != nullptr;
}

bool FSteamVoiceTransport::Send(uint64 Peer, const uint8* Data, int32 Size, int32 Flags) const
{
	ISteamNetworkingMessages* Messages = SteamNetworkingMessages();
	if (!Messages || Peer == 0 || !Data || Size <= 0)
	{
		return false;
	}
	return Messages->SendMessageToUser(MakeIdentity(Peer), Data, static_cast<uint32>(Size), Flags, Channel) == k_EResultOK;
}

bool FSteamVoiceTransport::SendVoice(uint64 Peer, const uint8* Data, int32 Size) const
{
	return Send(Peer, Data, Size, k_nSteamNetworkingSend_UnreliableNoDelay | k_nSteamNetworkingSend_AutoRestartBrokenSession);
}

bool FSteamVoiceTransport::SendHello(uint64 Peer, const uint8* Data, int32 Size) const
{
	return Send(Peer, Data, Size, k_nSteamNetworkingSend_Unreliable | k_nSteamNetworkingSend_AutoRestartBrokenSession);
}

int32 FSteamVoiceTransport::Receive(TFunctionRef<void(uint64 Sender, const uint8* Data, int32 Size)> Handler, int32 MaxMessages) const
{
	ISteamNetworkingMessages* Messages = SteamNetworkingMessages();
	if (!Messages)
	{
		return 0;
	}

	constexpr int32 BatchSize = 32;
	SteamNetworkingMessage_t* Batch[BatchSize];
	int32 Handled = 0;
	while (Handled < MaxMessages)
	{
		const int32 Count = Messages->ReceiveMessagesOnChannel(Channel, Batch, FMath::Min(BatchSize, MaxMessages - Handled));
		if (Count <= 0)
		{
			break;
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			SteamNetworkingMessage_t* Message = Batch[Index];
			Handler(Message->m_identityPeer.GetSteamID64(), static_cast<const uint8*>(Message->m_pData), Message->m_cbSize);
			Message->Release();
		}
		Handled += Count;
	}
	return Handled;
}

void FSteamVoiceTransport::AcceptSession(uint64 Peer) const
{
	if (ISteamNetworkingMessages* Messages = SteamNetworkingMessages())
	{
		Messages->AcceptSessionWithUser(MakeIdentity(Peer));
	}
}

void FSteamVoiceTransport::CloseSession(uint64 Peer) const
{
	if (ISteamNetworkingMessages* Messages = SteamNetworkingMessages())
	{
		Messages->CloseSessionWithUser(MakeIdentity(Peer));
	}
}

void FSteamVoiceTransport::OnSessionRequest(SteamNetworkingMessagesSessionRequest_t* Payload)
{
	// Steam's callback thread: only queue it, the worker decides (accept, keep pending, reject).
	const uint64 Peer = Payload ? Payload->m_identityRemote.GetSteamID64() : 0;
	if (Peer != 0)
	{
		SessionRequests.Enqueue(Peer);
	}
}

#else

FSteamVoiceTransport::FSteamVoiceTransport(int32 InChannel)
	: Channel(FMath::Max(InChannel, 0))
{
}

FSteamVoiceTransport::~FSteamVoiceTransport() = default;

bool FSteamVoiceTransport::IsAvailable() const
{
	return false;
}

bool FSteamVoiceTransport::SendVoice(uint64 /*Peer*/, const uint8* /*Data*/, int32 /*Size*/) const
{
	return false;
}

bool FSteamVoiceTransport::SendHello(uint64 /*Peer*/, const uint8* /*Data*/, int32 /*Size*/) const
{
	return false;
}

int32 FSteamVoiceTransport::Receive(TFunctionRef<void(uint64 Sender, const uint8* Data, int32 Size)> /*Handler*/, int32 /*MaxMessages*/) const
{
	return 0;
}

void FSteamVoiceTransport::AcceptSession(uint64 /*Peer*/) const
{
}

void FSteamVoiceTransport::CloseSession(uint64 /*Peer*/) const
{
}

#endif
