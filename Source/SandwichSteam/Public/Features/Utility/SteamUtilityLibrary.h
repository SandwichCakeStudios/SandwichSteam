// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamUtilityLibrary.generated.h"

/** Synchronous Blueprint getters for Steam utility data. They return defaults (empty, 0, false) while Steam is not active. */
UCLASS()
class SANDWICHSTEAM_API USteamUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "App ID Steam runs this game as. 0 when Steam is not active."))
	static int32 GetSteamAppId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "Two letter country code of the user's IP address. Empty when unknown."))
	static FString GetSteamIpCountry(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "Language of the Steam client UI as a Steam API language name, for example \"english\"."))
	static FString GetSteamUiLanguage(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "Language the user selected for this game in Steam, as a Steam API language name."))
	static FString GetSteamGameLanguage(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "Current UTC time according to the Steam servers."))
	static FDateTime GetSteamServerTime(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "Seconds since the game became the active Steam application."))
	static int32 GetSteamSecondsSinceAppActive(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the game runs on a Steam Deck."))
	static bool IsRunningOnSteamDeck(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the Steam client is in Big Picture mode."))
	static bool IsSteamBigPictureMode(const UObject* WorldContextObject);
};
