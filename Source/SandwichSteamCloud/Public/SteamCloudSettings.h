// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamCloudTypes.h"
#include "SteamCloudSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Cloud */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Cloud"))
class SANDWICHSTEAMCLOUD_API USteamCloudSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamCloudSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	/** What to do when the local and the cloud copy of a save slot differ. */
	UPROPERTY(Config, EditAnywhere, Category = "Cloud", meta = (ToolTip = "What to do when loading a slot whose local and cloud copies differ. Newest Wins keeps the copy saved last. Ask fires On Cloud Conflict and lets the game decide."))
	ESteamCloudConflictPolicy ConflictPolicy = ESteamCloudConflictPolicy::NewestWins;
};
