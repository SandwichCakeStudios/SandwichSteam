// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "Core/SteamSDK.h"
#include "SteamFriendsTypes.h"
#include "UObject/WeakObjectPtr.h"

class USteamFriendsSubsystem;

/**
 * Raw ISteamFriends side of the Friends feature. Query methods run on the game thread and must only be called while
 * Steam is Ready. The persona callback runs on Steam's callback thread: it copies the payload and dispatches to
 * USteamFriendsSubsystem on the game thread.
 */
class FSteamFriendsBackend
{
public:
	FSteamFriendsBackend(USteamFriendsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	FSteamFriendsBackend(const FSteamFriendsBackend&) = delete;
	FSteamFriendsBackend& operator=(const FSteamFriendsBackend&) = delete;

	/** Appends the Steam IDs of a list. */
	void CollectIds(ESteamFriendSource Source, TArray<FSteamId>& OutIds) const;

	/**
	 * Fills Out for one user. With bRequestIfMissing the user's data is requested from Steam when it is not there yet
	 * (needed for users who are not friends). Returns false while that data is still on its way (Out.bInfoLoaded = false).
	 */
	bool ReadInfo(FSteamId Id, bool bRequestIfMissing, FSteamFriendInfo& Out) const;

	int32 GetFriendCount() const;
	bool IsFriend(FSteamId Id) const;
	void ReadGroups(TArray<FSteamFriendGroup>& OutGroups) const;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamFriendsBackend, OnPersonaStateChange, PersonaStateChange_t, PersonaStateChangeCallback);
#endif

	TWeakObjectPtr<USteamFriendsSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};
