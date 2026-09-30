// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamCloudTypes.h"
#include "SteamCloudLibrary.generated.h"

class USaveGame;

/**
 * Steam Cloud saves for Blueprints. Everything is synchronous (Steam stores the file locally and uploads it in the
 * background), so it is safe to save right before quitting. Choose Steam Auto-Cloud or this API for a set of files, never both.
 */
UCLASS()
class SANDWICHSTEAMCLOUD_API USteamCloudLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "Saves a Save Game object into a slot: local file first, then Steam Cloud. If the user turned Steam Cloud off only the local file is written (still a success). Quota Exceeded keeps the local file. Slot names: letters, digits, _ and -."))
	static FSteamCloudSaveResult SaveGameToSteamCloud(const UObject* WorldContextObject, const FString& Slot, USaveGame* SaveGame);

	UFUNCTION(BlueprintCallable, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "Loads a slot. Picks the local or the cloud copy by the conflict policy of the settings and repairs the other copy. Outcome tells: Loaded, Not Found, Conflict (policy Ask: answer with Resolve Steam Cloud Conflict) or Corrupt."))
	static FSteamCloudLoadResult LoadGameFromSteamCloud(const UObject* WorldContextObject, const FString& Slot, USaveGame*& SaveGame);

	UFUNCTION(BlueprintCallable, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "Answers a Conflict: loads the copy you choose (cloud or local) and makes the other copy the same."))
	static FSteamCloudLoadResult ResolveSteamCloudConflict(const UObject* WorldContextObject, const FString& Slot, bool bUseCloud, USaveGame*& SaveGame);

	UFUNCTION(BlueprintCallable, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "Deletes the local file and the cloud copy of a slot."))
	static FSteamResult DeleteSteamCloudSlot(const UObject* WorldContextObject, const FString& Slot);

	UFUNCTION(BlueprintCallable, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "The slots that exist in Steam Cloud."))
	static TArray<FSteamCloudSlotInfo> ListSteamCloudSlots(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "True when Steam Cloud is on for the account and this game."))
	static bool IsSteamCloudEnabled(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Cloud", meta = (WorldContext = "WorldContextObject", ToolTip = "Total and free bytes of the game's Steam Cloud space. Returns false when Steam cannot tell."))
	static bool GetSteamCloudQuota(const UObject* WorldContextObject, FSteamCloudQuota& Quota);
};
