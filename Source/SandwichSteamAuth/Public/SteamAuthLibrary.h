// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamAuthTypes.h"
#include "SteamAuthLibrary.generated.h"

class APlayerState;

/**
 * Steam authentication of joining players for Blueprints. The events (On Steam Auth Result, On Steam Auth Failed) are on the
 * Steam Auth Subsystem: Get Game Instance Subsystem > Steam Auth Subsystem. Everything here is host side: a client has no results.
 * Needs Enable Steam Authentication (Project Settings > Plugins > Sandwich Steam - Auth) and Configure Steam.
 */
UCLASS()
class SANDWICHSTEAMAUTH_API USteamAuthLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|Auth", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the engine checks joining players with Steam (SteamAuth packet handler loaded) and Sandwich Steam reports the results."))
	static bool IsSteamAuthActive(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Auth", meta = (WorldContext = "WorldContextObject", ToolTip = "Where the player is in the Steam check on this host: Not Checked, Pending, Passed or Failed. Host only."))
	static ESteamAuthStatus GetSteamAuthStatus(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Auth", meta = (WorldContext = "WorldContextObject", ToolTip = "The last result Steam sent for the player. Returns false when there is none yet. Host only."))
	static bool GetSteamAuthResult(const UObject* WorldContextObject, FSteamId Player, FSteamAuthResult& Result);

	UFUNCTION(BlueprintPure, Category = "Steam|Auth", meta = (ToolTip = "True when Steam accepted the player. Players on the host's own machine are never checked and return true. Host only: clients get false for remote players (replicate your own flag to show a badge)."))
	static bool IsPlayerSteamAuthenticated(const APlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category = "Steam|Auth", meta = (WorldContext = "WorldContextObject", ToolTip = "Kicks a connected player with a reason. Host only. Meant for the Custom failure policy, from On Steam Auth Failed."))
	static FSteamResult KickSteamPlayer(const UObject* WorldContextObject, FSteamId Player, FText Reason);
};
