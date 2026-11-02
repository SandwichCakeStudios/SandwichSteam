// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceLobby.h"
#include "Core/SteamLog.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

namespace
{
	/** Updates in a row without the local player in the member list before a lobby counts as left (the player poll runs every 0.25 s). */
	constexpr int32 LeftAfterMissingUpdates = 4;
}

FSteamVoiceLobby::FSteamVoiceLobby()
{
	LobbyEnterCallback.Register(this, &FSteamVoiceLobby::OnLobbyEnter);
}

FSteamVoiceLobby::~FSteamVoiceLobby()
{
	LobbyEnterCallback.Unregister();
}

void FSteamVoiceLobby::OnLobbyEnter(LobbyEnter_t* Payload)
{
	// Steam's callback thread: only queue it, the game thread reads the members.
	if (Payload && Payload->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseSuccess && Payload->m_ulSteamIDLobby != 0)
	{
		Entered.Enqueue(Payload->m_ulSteamIDLobby);
	}
}

void FSteamVoiceLobby::Update(uint64 LocalId, TSet<int64>& OutMembers)
{
	uint64 EnteredLobby = 0;
	while (Entered.Dequeue(EnteredLobby))
	{
		if (!Lobbies.ContainsByPredicate([EnteredLobby](const FTrackedLobby& Tracked) { return Tracked.Lobby == EnteredLobby; }))
		{
			Lobbies.Add({ EnteredLobby, 0 });
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: entered lobby %llu; its members are heard while there is no networked game (Allow Voice In Lobby)."), EnteredLobby);
		}
	}

	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || LocalId == 0)
	{
		return;
	}

	TArray<int64, TInlineAllocator<16>> Members;
	for (int32 Index = Lobbies.Num() - 1; Index >= 0; --Index)
	{
		FTrackedLobby& Tracked = Lobbies[Index];
		const CSteamID LobbyId(Tracked.Lobby);
		const int32 Count = Matchmaking->GetNumLobbyMembers(LobbyId);

		Members.Reset();
		bool bLocalIn = false;
		for (int32 Member = 0; Member < Count; ++Member)
		{
			const uint64 Id = Matchmaking->GetLobbyMemberByIndex(LobbyId, Member).ConvertToUint64();
			if (Id == LocalId)
			{
				bLocalIn = true;
			}
			else if (Id != 0)
			{
				Members.Add(static_cast<int64>(Id));
			}
		}

		if (bLocalIn)
		{
			Tracked.MissingUpdates = 0;
			OutMembers.Append(Members);
		}
		else if (++Tracked.MissingUpdates >= LeftAfterMissingUpdates)
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: left lobby %llu; its members are no longer heard."), Tracked.Lobby);
			Lobbies.RemoveAt(Index);
		}
	}
}

#else

FSteamVoiceLobby::FSteamVoiceLobby() = default;
FSteamVoiceLobby::~FSteamVoiceLobby() = default;

void FSteamVoiceLobby::Update(uint64 /*LocalId*/, TSet<int64>& /*OutMembers*/)
{
}

#endif
