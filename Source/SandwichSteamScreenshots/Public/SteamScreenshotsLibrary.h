// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamScreenshotsLibrary.generated.h"

/** Steam screenshots for Blueprints. */
UCLASS()
class SANDWICHSTEAMSCREENSHOTS_API USteamScreenshotsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots", meta = (WorldContext = "WorldContextObject", ToolTip = "Asks Steam to take a screenshot, as if the screenshot key was pressed."))
	static FSteamResult TriggerSteamScreenshot(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots", meta = (WorldContext = "WorldContextObject", ToolTip = "Captures the game viewport (without the Steam overlay) and adds it to the Steam screenshot library. The Screenshot Ready event fires when Steam has it."))
	static FSteamResult CaptureViewportToSteam(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots", meta = (WorldContext = "WorldContextObject", ToolTip = "Sets the location text (for example the level name) that Steam shows with screenshots. Empty clears it."))
	static void SetSteamScreenshotLocation(const UObject* WorldContextObject, const FString& Location);

	UFUNCTION(BlueprintCallable, Category = "Steam|Screenshots", meta = (WorldContext = "WorldContextObject", ToolTip = "Tags a Steam user on the next screenshot (at most 32)."))
	static FSteamResult TagSteamScreenshotUser(const UObject* WorldContextObject, FSteamId User);

	UFUNCTION(BlueprintPure, Category = "Steam|Screenshots", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the game owns the Steam screenshot key (hook mode in the settings)."))
	static bool IsSteamScreenshotHookActive(const UObject* WorldContextObject);
};
