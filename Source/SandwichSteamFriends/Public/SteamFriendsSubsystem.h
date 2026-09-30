// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamChangeBatch.h"
#include "Core/SteamFeatureSubsystem.h"
#include "SteamFriendsTypes.h"
#include "SteamFriendsSubsystem.generated.h"

class FSteamFriendsBackend;
class FSteamCallbackDispatcher;
class USteamOverlaySubsystem;

/**
 * The friends list of the local Steam user (ISteamFriends). Client only.
 *
 * Reading a list is asynchronous only because Steam may still be delivering the names of users who are not friends
 * (recent players); a list of friends normally completes at once. Persona changes are coalesced: a login delivers one
 * change per friend, and this feature turns them into one event per friend and frame.
 *
 * Avatars are not part of the list: ask the User feature with the Steam ID when a widget needs one.
 * Steam has no call to accept or send a friend request from a game. Use the overlay (Open Add Friend Dialog).
 * Completion delegates run on the game thread. They may run before the call returns, and they are not called when the
 * feature shuts down first (the Blueprint async node reports Steam.Error.Cancelled instead).
 */
UCLASS()
class SANDWICHSTEAMFRIENDS_API USteamFriendsSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Friends subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamFriendsSubsystem* Get(const UObject* WorldContext);

	USteamFriendsSubsystem();
	virtual ~USteamFriendsSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/**
	 * Reads a list of users (friends, blocked or recent players), narrowed by Filter and sorted: users who are online
	 * first, then by name. A false Start result means the delegate will never run.
	 */
	FSteamResult ReadFriends(ESteamFriendSource Source, ESteamFriendFilter Filter, FSteamReadFriendsDelegate OnComplete);

	/** Number of friends (immediate friends only). 0 while inactive. */
	int32 GetFriendCount() const;

	/** True when the user is a friend of the local user. */
	bool IsFriend(FSteamId Id) const;

	/** Reads one user right now, without waiting. Returns false while inactive. Info.bInfoLoaded tells whether Steam has the data. */
	bool GetFriendInfo(FSteamId Id, FSteamFriendInfo& OutInfo) const;

	/** The friend groups (tags) of the local user. Empty when the SDK has none or the feature is inactive. */
	TArray<FSteamFriendGroup> GetFriendGroups() const;

	/** Overlay pages of a user. They delegate to the Overlay feature of the core. */
	FSteamResult OpenFriendsList();
	FSteamResult OpenProfile(FSteamId Id);
	FSteamResult OpenChat(FSteamId Id);
	FSteamResult OpenAddFriend(FSteamId Id);

	/** A friend was added, removed or blocked. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Friends", meta = (ToolTip = "Called when a friend was added, removed or blocked. At most once per frame."))
	FOnSteamFriendsListChanged OnFriendsListChanged;

	/** A friend changed: name, status, game or avatar. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Friends", meta = (ToolTip = "Called when a friend changed name, status, game or avatar. ChangeFlags is a mask of Steam Friend Change. At most once per friend and frame."))
	FOnSteamFriendStateChanged OnFriendStateChanged;

	/** Number of persona changes Steam delivered / of events that were broadcast, since the feature became active. */
	int32 GetPersonaChangeCount() const { return PersonaChanges; }
	int32 GetBroadcastCount() const { return Broadcasts; }

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Friends.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamFriendsBackend;
	struct FPendingRead;

	/** Game thread, called through the dispatcher by the backend. Flags is a mask of ESteamFriendChange. */
	void HandlePersonaChange(FSteamId UserId, uint32 Flags);

	/** Broadcasts the coalesced changes. Runs one dispatcher round after the first change. */
	void FlushChanges();

	void FinishRead(uint32 ReadId, bool bTimedOut);
	void CompleteRead(const TSharedRef<FPendingRead>& Read);
	FSteamResult RequireOverlay(USteamOverlaySubsystem*& OutOverlay) const;

	TSharedPtr<FSteamFriendsBackend> Backend;
	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	TArray<TSharedRef<FPendingRead>> PendingReads;
	uint32 NextReadId = 1;

	TSteamChangeBatch<FSteamId> Changes;

	int32 PersonaChanges = 0;
	int32 Broadcasts = 0;
};
