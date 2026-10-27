// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Core/SteamSDK.h"

/**
 * The Steam lobbies the local player is in, for voice in a lobby without a game (Allow Voice In Lobby).
 *
 * Steam keeps no list of the lobbies a player is in: the LobbyEnter_t callback (Steam's callback thread) only queues every lobby entered,
 * and Update (game thread) keeps the ones whose member list still has the local player. Leaving, a kick or a closed lobby drop out on a
 * later update. Works for every lobby: the Sessions feature's, the Online Subsystem's, the game's own.
 */
class FSteamVoiceLobby
{
public:
	FSteamVoiceLobby();
	~FSteamVoiceLobby();

	FSteamVoiceLobby(const FSteamVoiceLobby&) = delete;
	FSteamVoiceLobby& operator=(const FSteamVoiceLobby&) = delete;

	/** Game thread: takes the lobbies entered since the last call, forgets the ones LocalId left, and adds every other member of the rest to OutMembers. */
	void Update(uint64 LocalId, TSet<int64>& OutMembers);

	/** The lobbies the local player is in, as of the last update. */
	int32 NumLobbies() const { return Lobbies.Num(); }

private:
	struct FTrackedLobby
	{
		uint64 Lobby = 0;
		/** Updates in a row the local player was not in the member list (Steam may fill the list a moment after the enter). */
		int32 MissingUpdates = 0;
	};

#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamVoiceLobby, OnLobbyEnter, LobbyEnter_t, LobbyEnterCallback);
#endif

	TQueue<uint64, EQueueMode::Mpsc> Entered;
	TArray<FTrackedLobby> Lobbies;
};
