// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamChangeBatch.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Data/SteamAppDefinition.h"
#include "SteamPresenceTypes.h"
#include "SteamPresenceSubsystem.generated.h"

class FSteamPresenceBackend;
class FSteamCallbackDispatcher;
class FSteamPresenceBatch;

/**
 * Steam rich presence: the status friends see next to the game in their friends list, and the "join game" connect string.
 * Client only.
 *
 * Statuses are rows of the Steam App Definition (tag, localization token, the keys the text substitutes). Setting a
 * status sets steam_display plus its keys. Every change is staged and sent once per frame: setting several keys, or a
 * value Steam already has, costs at most one round of Steam calls. Steam allows 30 keys, 64 bytes per key and 256 bytes
 * per value; a change that breaks a limit is refused with Steam.Error.InvalidArgument / QuotaExceeded and nothing of it is sent.
 *
 * When the feature is deactivated (Steam went offline) the keys are kept and sent again when it becomes active.
 */
UCLASS()
class SANDWICHSTEAMPRESENCE_API USteamPresenceSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Presence subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamPresenceSubsystem* Get(const UObject* WorldContext);

	USteamPresenceSubsystem();
	virtual ~USteamPresenceSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/**
	 * Shows the status of a row of the App Definition: sets steam_display to its token and each argument as a key
	 * (the substitutions, for example map=Harbor). Keys the previous status set and this one does not use are removed.
	 */
	FSteamResult SetPresenceByTag(const FGameplayTag& StatusTag, const TMap<FName, FString>& Args);

	/**
	 * The same with the localization token itself (for example #Status_InMatch), no App Definition row needed. The token must start
	 * with # and exist in the localization file uploaded to Steamworks, or friends see nothing.
	 */
	FSteamResult SetPresenceByToken(const FString& Token, const TMap<FName, FString>& Args);

	/** Sets a free key (for example "status", the plain text that needs no localization file). An empty value removes the key. */
	FSteamResult SetPresenceValue(FName Key, const FString& Value);

	/** Removes a key. */
	FSteamResult RemovePresenceValue(FName Key);

	/** Sets steam_player_group and steam_player_group_size, which Steam uses to group friends in the same party or match. An empty GroupId removes both. */
	FSteamResult SetGroup(const FString& GroupId, int32 Size);

	/** Sets the connect key: the command line Steam passes to the game when a friend clicks Join Game. An empty string removes it. */
	FSteamResult SetConnectString(const FString& Connect);

	/** Removes every key. */
	FSteamResult ClearPresence();

	/** One key of a friend. Empty when not set. */
	FString GetFriendPresenceValue(FSteamId Friend, FName Key) const;

	/** Every key of a friend. */
	TMap<FString, FString> GetFriendPresence(FSteamId Friend) const;

	/** Asks Steam for the presence of a user. Friends are updated without it. */
	void RequestFriendPresence(FSteamId Friend);

	/** The App Definition the tags are resolved with. Null when none is assigned (then only free keys work). */
	const USteamAppDefinition* GetDefinition() const { return Definition; }

	/** Called when a friend changed their rich presence. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Presence", meta = (ToolTip = "Called when the rich presence of a friend changed. At most once per friend and frame."))
	FOnSteamFriendPresenceChanged OnFriendPresenceChanged;

	/** Flushes that sent something / Steam calls made, since the feature became active. */
	int32 GetFlushCount() const { return FlushCount; }
	int32 GetSteamCallCount() const { return SteamCalls; }

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Presence.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamPresenceBackend;

	/** Game thread, called through the dispatcher by the backend. */
	void HandleFriendPresenceUpdate(FSteamId Friend);

	void FlushFriendChanges();
	void ScheduleFlush();
	void Flush();

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	TSharedPtr<FSteamPresenceBackend> Backend;
	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	/** The keys of the local user. Outlives Backend so a reactivation can send everything again. */
	TSharedPtr<FSteamPresenceBatch> Batch;

	/** Keys the last status set from its row. Used to remove them when another status does not use them. */
	TSet<FString> LastStatusKeys;

	TSteamChangeBatch<FSteamId> FriendChanges;

	bool bFlushScheduled = false;
	int32 FlushCount = 0;
	int32 SteamCalls = 0;
};
