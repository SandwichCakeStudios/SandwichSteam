// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "Core/SteamSDK.h"
#include "SteamPresenceBatch.h"
#include "UObject/WeakObjectPtr.h"

class USteamPresenceSubsystem;

/**
 * Raw ISteamFriends rich presence calls. Methods run on the game thread and must only be called while Steam is Ready.
 * The friend presence callback runs on Steam's callback thread: it copies the payload and dispatches to the subsystem.
 */
class FSteamPresenceBackend
{
public:
	FSteamPresenceBackend(USteamPresenceSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	FSteamPresenceBackend(const FSteamPresenceBackend&) = delete;
	FSteamPresenceBackend& operator=(const FSteamPresenceBackend&) = delete;

	/** Sends one flush to Steam. Returns the number of calls Steam refused. */
	int32 Apply(const FSteamPresenceBatch::FFlush& Flush, int32& OutCallCount) const;

	/** ClearRichPresence. */
	void ClearAll() const;

	/** One key of a friend (empty when not set). */
	FString GetFriendValue(FSteamId Friend, const FString& Key) const;

	/** Every key of a friend. */
	void GetFriendValues(FSteamId Friend, TMap<FString, FString>& OutValues) const;

	/** Asks Steam for the presence of a user (friends are updated by Steam without this). */
	void RequestFriend(FSteamId Friend) const;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamPresenceBackend, OnFriendRichPresenceUpdate, FriendRichPresenceUpdate_t, FriendPresenceCallback);
#endif

	TWeakObjectPtr<USteamPresenceSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};
