// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "SteamCloudTypes.generated.h"

namespace SandwichSteam::Cloud
{
	/** Extension of the local and cloud file of a slot. */
	inline const TCHAR* const SlotExtension = TEXT(".steamsave");

	/** Longest slot name. */
	constexpr int32 MaxSlotNameLength = 100;
}

/** What to do when the local and the cloud copy of a slot differ. */
UENUM(BlueprintType)
enum class ESteamCloudConflictPolicy : uint8
{
	/** The copy with the later save time wins (a tie keeps the local copy). */
	NewestWins,
	/** The local copy always wins and replaces the cloud copy. */
	PreferLocal,
	/** The cloud copy always wins and replaces the local copy. */
	PreferCloud,
	/** Load fails with Conflict and On Cloud Conflict fires with both copies. The game decides (Resolve Cloud Conflict). */
	Ask
};

/** How a load ended. */
UENUM(BlueprintType)
enum class ESteamCloudLoadOutcome : uint8
{
	/** The data was loaded. */
	Loaded,
	/** Neither the local nor the cloud copy exists. */
	NotFound,
	/** The copies differ and the policy is Ask. Nothing was loaded. */
	Conflict,
	/** A copy exists but failed its integrity check, and there is no good copy. */
	Corrupt
};

/** The decision the pure conflict logic makes. */
enum class ESteamCloudResolution : uint8
{
	NothingFound,
	UseLocal,
	UseCloud,
	InSync,
	Ask
};

/** Header data of one copy of a slot. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMCLOUD_API FSteamCloudSaveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when this copy exists and passed its integrity check."))
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "When the copy was saved (Unix time, UTC)."))
	int64 UnixTime = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Size of the save data in bytes."))
	int32 PayloadSize = 0;
};

/** What a save did. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMCLOUD_API FSteamCloudSaveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Success, or why it failed. Quota Exceeded means the local copy was saved but the cloud copy was not."))
	FSteamResult Result;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the local file was written."))
	bool bSavedLocal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the cloud copy was written. False when the user turned Steam Cloud off (that is not an error) or the write failed."))
	bool bSavedCloud = false;
};

/** What a load did. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMCLOUD_API FSteamCloudLoadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Success (also for Not Found and Conflict), or why the load failed."))
	FSteamResult Result;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Loaded, Not Found, Conflict or Corrupt."))
	ESteamCloudLoadOutcome Outcome = ESteamCloudLoadOutcome::NotFound;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the loaded data came from the cloud copy."))
	bool bFromCloud = false;
};

/** One save slot in the cloud. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMCLOUD_API FSteamCloudSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Slot name."))
	FString Slot;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Size of the cloud file in bytes."))
	int64 FileSize = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Last change of the cloud file (Unix time, UTC)."))
	int64 FileTime = 0;
};

/** Quota of the game's Steam Cloud space. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMCLOUD_API FSteamCloudQuota
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Total bytes the game may store."))
	int64 TotalBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Bytes still free."))
	int64 AvailableBytes = 0;
};

/** Called when a load found different local and cloud copies and the conflict policy is Ask. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamCloudConflict, const FString&, Slot, const FSteamCloudSaveInfo&, Local, const FSteamCloudSaveInfo&, Cloud);
