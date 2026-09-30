// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamAchievementsSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Achievements */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Achievements"))
class SANDWICHSTEAMACHIEVEMENTS_API USteamAchievementsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamAchievementsSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	/** Change a stat that has an achievement attached and Steam shows the progress toast on its own. */
	UPROPERTY(Config, EditAnywhere, Category = "Achievements", meta = (ToolTip = "When a stat with an achievement attached (Progress Stat in the App Definition) changes, show the Steam progress toast automatically."))
	bool bAutoIndicateProgressFromStats = true;

	/** The progress toast is shown each time progress passes another step of this many percent. */
	UPROPERTY(Config, EditAnywhere, Category = "Achievements", meta = (ClampMin = "1", ClampMax = "50", ToolTip = "The Steam progress toast is shown each time progress passes another step of this many percent, so quick stat changes do not spam the screen."))
	int32 ProgressNotifyStepPercent = 10;
};
