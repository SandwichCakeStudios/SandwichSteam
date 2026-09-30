// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamScreenshotsSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Screenshots */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Screenshots"))
class SANDWICHSTEAMSCREENSHOTS_API USteamScreenshotsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamScreenshotsSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	/** Take over the Steam screenshot key (F12 by default). */
	UPROPERTY(Config, EditAnywhere, Category = "Screenshots", meta = (ToolTip = "When on, the game answers the Steam screenshot key itself: it captures the game viewport (without the Steam overlay) and hands the image to Steam, with the location and tagged users you set. When off, Steam captures the screen itself."))
	bool bHookScreenshots = false;
};
