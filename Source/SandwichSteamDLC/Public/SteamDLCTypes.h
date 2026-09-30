// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SteamDLCTypes.generated.h"

/** One DLC as Steam reports it, joined with the App Definition row when there is one. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMDLC_API FSteamDLCInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam App ID of the DLC."))
	int32 AppId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Name of the DLC from Steam."))
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Gameplay tag of the DLC in the Steam App Definition. Empty when it has no row."))
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the DLC can be bought or installed (it is released and visible)."))
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the local user owns the DLC."))
	bool bOwned = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the DLC files are installed."))
	bool bInstalled = false;
};

/** Download state of a DLC. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMDLC_API FSteamDLCProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True while Steam downloads the DLC."))
	bool bDownloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Bytes downloaded so far."))
	int64 BytesDownloaded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Bytes to download in total."))
	int64 BytesTotal = 0;

	/** 0..1, 0 when nothing is downloading. */
	float GetFraction() const { return (bDownloading && BytesTotal > 0) ? static_cast<float>(static_cast<double>(BytesDownloaded) / static_cast<double>(BytesTotal)) : 0.f; }
};

/** Called when a DLC finished installing. Tag is empty for DLC without an App Definition row. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamDLCInstalled, int32, AppId, FGameplayTag, Tag);
