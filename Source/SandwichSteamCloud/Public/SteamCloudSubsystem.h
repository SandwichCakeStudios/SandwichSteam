// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "SteamCloudTypes.h"
#include "SteamCloudSubsystem.generated.h"

class FSteamCloudBackend;
class USaveGame;

/**
 * Steam Cloud saves (ISteamRemoteStorage). Client only.
 *
 * A save slot is one file. Saving writes the local file first, then the cloud copy; both hold the same bytes: a small
 * header (format version, save time, CRC) and the payload, so corruption is detected on load. Loading reads both copies,
 * ignores a corrupt one, and picks the winner by the conflict policy of the settings (Newest Wins, Prefer Local, Prefer
 * Cloud or Ask). The other copy is brought up to date, so after a load both are the same.
 *
 * Every call is synchronous. That is safe and fast: Steam stores the file in its local cloud cache and uploads it in the
 * background, so a save right before quitting works without any special handling. Call it from your quit flow.
 *
 * If the user turned Steam Cloud off (account or game), saving keeps the local file only and succeeds; that is a user
 * choice, not an error. A full cloud (quota) keeps the local file and fails with Steam.Error.QuotaExceeded.
 *
 * Steam Auto-Cloud and this API must not manage the same files. Use one of them for a set of files, never both.
 */
UCLASS()
class SANDWICHSTEAMCLOUD_API USteamCloudSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Cloud subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamCloudSubsystem* Get(const UObject* WorldContext);

	USteamCloudSubsystem();
	virtual ~USteamCloudSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** Saves raw bytes into a slot (local file, then cloud). */
	FSteamCloudSaveResult SaveBytes(const FString& Slot, TConstArrayView<uint8> Payload);

	/** Loads raw bytes from a slot. See the class comment for how the copies are chosen. */
	FSteamCloudLoadResult LoadBytes(const FString& Slot, TArray<uint8>& OutPayload);

	/** After a Conflict outcome: loads the copy the game chose and makes the other copy the same. */
	FSteamCloudLoadResult ResolveConflict(const FString& Slot, bool bUseCloud, TArray<uint8>& OutPayload);

	/** USaveGame helpers: serialize with the engine (UGameplayStatics) and use the byte functions above. */
	FSteamCloudSaveResult SaveGameToCloud(const FString& Slot, USaveGame* SaveGame);
	FSteamCloudLoadResult LoadGameFromCloud(const FString& Slot, USaveGame*& OutSaveGame);
	FSteamCloudLoadResult ResolveConflictToSaveGame(const FString& Slot, bool bUseCloud, USaveGame*& OutSaveGame);

	/** Deletes the local file and the cloud copy. Succeeds when there was nothing to delete. */
	FSteamResult DeleteSlot(const FString& Slot);

	/** Deletes only the local file (a new device, or a test of the cloud copy). */
	FSteamResult DeleteLocalSlot(const FString& Slot);

	/** Header data of both copies of a slot, without loading anything. */
	void GetSlotInfo(const FString& Slot, FSteamCloudSaveInfo& OutLocal, FSteamCloudSaveInfo& OutCloud) const;

	/** The slots that exist in the cloud (files that end with the slot extension). */
	TArray<FSteamCloudSlotInfo> ListCloudSlots() const;

	/** True when Steam Cloud is on for the account and this game. */
	bool IsCloudEnabled() const;

	/** Quota of the game's cloud space. False when Steam cannot tell. */
	bool GetQuota(FSteamCloudQuota& OutQuota) const;

	/** Path of the local file of a slot. */
	static FString GetLocalPath(const FString& Slot);

	/** Called when a load found different copies and the policy is Ask. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Cloud", meta = (ToolTip = "Called when loading a slot found different local and cloud copies and the conflict policy is Ask. Choose one with Resolve Steam Cloud Conflict."))
	FOnSteamCloudConflict OnCloudConflict;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Cloud.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	struct FCopy;

	/** Reads and checks one copy. bExisted tells a missing file (fine) from a corrupt one. */
	FCopy ReadLocal(const FString& Slot) const;
	FCopy ReadCloud(const FString& Slot) const;

	FSteamCloudLoadResult Finish(const FString& Slot, ESteamCloudResolution Resolution, const FCopy& Local, const FCopy& Cloud, TArray<uint8>& OutPayload);
	bool WriteLocal(const FString& Slot, const TArray<uint8>& Bytes) const;
	FSteamResult WriteCloud(const FString& Slot, const TArray<uint8>& Bytes, bool& bOutWritten) const;
	FSteamResult ValidateSlot(const FString& Slot) const;

	TSharedPtr<FSteamCloudBackend> Backend;
};
