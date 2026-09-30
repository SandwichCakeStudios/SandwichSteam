// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamStatsSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Stats */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Stats"))
class SANDWICHSTEAMSTATS_API USteamStatsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamStatsSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	/** Seconds that changed stats are collected before they are uploaded to Steam in one StoreStats call. */
	UPROPERTY(Config, EditAnywhere, Category = "Stats", meta = (ClampMin = "1.0", ClampMax = "300.0", ToolTip = "Changed stats are collected for this many seconds and uploaded to Steam in one call. Unlocking an achievement uploads immediately. Higher values save bandwidth, lower values lose less on a crash."))
	float FlushIntervalSeconds = 5.0f;
};
