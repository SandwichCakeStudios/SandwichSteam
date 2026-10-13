// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamAuthTypes.h"
#include "SteamAuthSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Auth */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Auth"))
class SANDWICHSTEAMAUTH_API USteamAuthSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamAuthSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (DisplayName = "Enable Steam Authentication", ToolTip = "Steam checks every player who joins your hosted game (owns the game, no VAC or publisher ban). Configure Steam writes the SteamAuth packet handler line into DefaultEngine.ini. Host and clients must use the same setting, so ship one build. Restart the editor after Configure Steam."))
	bool bEnableSteamAuthentication = false;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "Kick: the engine kicks players who fail. Custom: nobody is kicked automatically; On Steam Auth Failed fires and your game decides (use Kick Steam Player)."))
	ESteamAuthFailurePolicy FailurePolicy = ESteamAuthFailurePolicy::Kick;
};
