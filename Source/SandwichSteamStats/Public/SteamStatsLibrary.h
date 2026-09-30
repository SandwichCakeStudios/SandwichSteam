// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamStatsLibrary.generated.h"

/**
 * Blueprint access to Steam stats by gameplay tag (rows of the Steam App Definition in the Sandwich Steam settings).
 * Every node returns a Steam Result. Writes made before the stats arrived from Steam are queued.
 */
UCLASS()
class SANDWICHSTEAMSTATS_API USteamStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", ToolTip = "True once Steam delivered the stats of the local user."))
	static bool AreSteamStatsReady(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Reads an integer stat."))
	static FSteamResult GetSteamStatInt(const UObject* WorldContextObject, FGameplayTag StatTag, int32& Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Reads a float (or average rate) stat."))
	static FSteamResult GetSteamStatFloat(const UObject* WorldContextObject, FGameplayTag StatTag, float& Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Sets an integer stat. Clamped to the range of the stat definition. Uploaded to Steam in the background."))
	static FSteamResult SetSteamStatInt(const UObject* WorldContextObject, FGameplayTag StatTag, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Sets a float stat. Clamped to the range of the stat definition. Uploaded to Steam in the background."))
	static FSteamResult SetSteamStatFloat(const UObject* WorldContextObject, FGameplayTag StatTag, float Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Adds to an integer stat (use a negative value to subtract). Clamped to the range of the stat definition."))
	static FSteamResult AddSteamStatInt(const UObject* WorldContextObject, FGameplayTag StatTag, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Adds to a float stat (use a negative value to subtract). Clamped to the range of the stat definition."))
	static FSteamResult AddSteamStatFloat(const UObject* WorldContextObject, FGameplayTag StatTag, float Delta);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Stat", ToolTip = "Updates an average-rate stat: Count events happened during SessionSeconds of play."))
	static FSteamResult UpdateSteamAvgRateStat(const UObject* WorldContextObject, FGameplayTag StatTag, float Count, float SessionSeconds);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (WorldContext = "WorldContextObject", ToolTip = "Uploads changed stats now instead of waiting for the flush interval. Use the Store Steam Stats node to wait for the result."))
	static FSteamResult StoreSteamStatsNow(const UObject* WorldContextObject);

	// By Name: the same with the API name from Steamworks instead of a tag. No App Definition row needed (then there is no range clamping).

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Steam Stat Int By Name", Keywords = "get steam stat int api name", ToolTip = "Reads an integer stat by its API name in Steamworks."))
	static FSteamResult GetSteamStatIntByName(const UObject* WorldContextObject, FName StatName, int32& Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Steam Stat Float By Name", Keywords = "get steam stat float api name", ToolTip = "Reads a float (or average rate) stat by its API name in Steamworks."))
	static FSteamResult GetSteamStatFloatByName(const UObject* WorldContextObject, FName StatName, float& Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Set Steam Stat Int By Name", Keywords = "set steam stat int api name", ToolTip = "Sets an integer stat by its API name in Steamworks. Clamped only when the App Definition has a row for it. Uploaded to Steam in the background."))
	static FSteamResult SetSteamStatIntByName(const UObject* WorldContextObject, FName StatName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Set Steam Stat Float By Name", Keywords = "set steam stat float api name", ToolTip = "Sets a float stat by its API name in Steamworks. Clamped only when the App Definition has a row for it. Uploaded to Steam in the background."))
	static FSteamResult SetSteamStatFloatByName(const UObject* WorldContextObject, FName StatName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Add Steam Stat Int By Name", Keywords = "add steam stat int api name increment", ToolTip = "Adds to an integer stat by its API name in Steamworks (use a negative value to subtract)."))
	static FSteamResult AddSteamStatIntByName(const UObject* WorldContextObject, FName StatName, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Add Steam Stat Float By Name", Keywords = "add steam stat float api name increment", ToolTip = "Adds to a float stat by its API name in Steamworks (use a negative value to subtract)."))
	static FSteamResult AddSteamStatFloatByName(const UObject* WorldContextObject, FName StatName, float Delta);

	UFUNCTION(BlueprintCallable, Category = "Steam|Stats|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Update Steam Avg Rate Stat By Name", Keywords = "update steam avg average rate stat api name", ToolTip = "Updates an average-rate stat by its API name in Steamworks: Count events happened during SessionSeconds of play."))
	static FSteamResult UpdateSteamAvgRateStatByName(const UObject* WorldContextObject, FName StatName, float Count, float SessionSeconds);
};
