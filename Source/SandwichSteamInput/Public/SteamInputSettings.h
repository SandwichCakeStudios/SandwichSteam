// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "SteamInputTypes.h"
#include "SteamInputSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Input */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Input"))
class SANDWICHSTEAMINPUT_API USteamInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamInputSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (Categories = "Steam.Input.ActionSet", ToolTip = "Action set that is active when the game starts. Empty = the first action set (not layer) of the Steam App Definition."))
	FGameplayTag DefaultActionSet;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "Add the mapping context of the active action set to the first local player, and swap it when the action set changes. Turn off when you manage your Enhanced Input contexts yourself."))
	bool bAutoActivateContexts = true;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0.25", ClampMax = "5.0", ToolTip = "Seconds between checks for a newly connected controller while none is connected. With a controller connected the feature runs every frame. Nothing else runs while idle."))
	float IdlePollSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "16", ToolTip = "Most controllers that drive input at the same time (local players)."))
	int32 MaxControllers = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Glyphs", meta = (ToolTip = "Size of the button glyph images."))
	ESteamGlyphSize GlyphSize = ESteamGlyphSize::Medium;

	UPROPERTY(Config, EditAnywhere, Category = "Glyphs", meta = (ClampMin = "1", ClampMax = "512", ToolTip = "Most glyph textures kept in memory. The least recently used are dropped first."))
	int32 GlyphCacheSize = 32;

	UPROPERTY(Config, EditAnywhere, Category = "Testing", meta = (FilePathFilter = "vdf", RelativeToGameDir, ToolTip = "Optional. A Steam Input action file (game_actions_<AppId>.vdf, Generate Steam Input Actions File on the App Definition) that Steam loads instead of the one configured on the partner site. For testing with an App ID you do not own, for example 480. Leave empty for a release."))
	FFilePath ActionManifest;
};
