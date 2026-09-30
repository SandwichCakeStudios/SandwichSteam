// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamAchievementTypes.h"
#include "SteamAchievementsLibrary.generated.h"

/**
 * Blueprint access to Steam achievements by gameplay tag (rows of the Steam App Definition in the Sandwich Steam settings).
 * Do not mix with the engine's Online Subsystem achievement nodes for the same achievements.
 */
UCLASS()
class SANDWICHSTEAMACHIEVEMENTS_API USteamAchievementsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Achievement", ToolTip = "Unlocks an achievement and shows the Steam toast. Already unlocked achievements succeed without doing anything. Before Steam delivered the stats the unlock is queued."))
	static FSteamResult UnlockSteamAchievement(const UObject* WorldContextObject, FGameplayTag AchievementTag);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Achievement", DevelopmentOnly, ToolTip = "Locks an achievement again. For testing only: fails in Shipping builds."))
	static FSteamResult ClearSteamAchievement(const UObject* WorldContextObject, FGameplayTag AchievementTag);

	UFUNCTION(BlueprintPure, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Achievement", ToolTip = "True when the local user unlocked the achievement. False when unknown or the stats are not ready."))
	static bool IsSteamAchievementUnlocked(const UObject* WorldContextObject, FGameplayTag AchievementTag);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Achievement", ToolTip = "Localized name and description, unlock state and progress of one achievement."))
	static FSteamResult GetSteamAchievementInfo(const UObject* WorldContextObject, FGameplayTag AchievementTag, FSteamAchievementInfo& Info);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", ToolTip = "Every achievement of the game with name, description, unlock state and progress. Empty until the stats are ready."))
	static TArray<FSteamAchievementInfo> GetAllSteamAchievements(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Achievement", ToolTip = "Shows the Steam progress toast for an achievement with a progress stat. Throttled, and never shown at or above the maximum. Changing the progress stat does this automatically."))
	static FSteamResult IndicateSteamAchievementProgress(const UObject* WorldContextObject, FGameplayTag AchievementTag, int32 CurrentValue);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Achievement", ToolTip = "Percentage of all players that unlocked the achievement (0-100). Returns false until the Request Steam Achievement Percentages node succeeded."))
	static bool GetSteamAchievementGlobalPercent(const UObject* WorldContextObject, FGameplayTag AchievementTag, float& Percent);

	// By Name: the same with the API name from Steamworks instead of a tag. No App Definition row needed.

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Unlock Steam Achievement By Name", Keywords = "unlock steam achievement api name", ToolTip = "Unlocks an achievement by its API name in Steamworks and shows the Steam toast. Already unlocked achievements succeed without doing anything. Before Steam delivered the stats the unlock is queued."))
	static FSteamResult UnlockSteamAchievementByName(const UObject* WorldContextObject, FName AchievementName);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements|By Name", meta = (WorldContext = "WorldContextObject", DevelopmentOnly, DisplayName = "Clear Steam Achievement By Name", Keywords = "clear lock reset steam achievement api name", ToolTip = "Locks an achievement again by its API name in Steamworks. For testing only: fails in Shipping builds."))
	static FSteamResult ClearSteamAchievementByName(const UObject* WorldContextObject, FName AchievementName);

	UFUNCTION(BlueprintPure, Category = "Steam|Achievements|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Is Steam Achievement Unlocked By Name", Keywords = "is steam achievement unlocked api name", ToolTip = "True when the local user unlocked the achievement with this API name. False when unknown or the stats are not ready."))
	static bool IsSteamAchievementUnlockedByName(const UObject* WorldContextObject, FName AchievementName);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Steam Achievement Info By Name", Keywords = "get steam achievement info api name", ToolTip = "Localized name and description, unlock state and progress of the achievement with this API name. Progress needs a row with a Progress Stat in the App Definition."))
	static FSteamResult GetSteamAchievementInfoByName(const UObject* WorldContextObject, FName AchievementName, FSteamAchievementInfo& Info);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Indicate Steam Achievement Progress By Name", Keywords = "indicate steam achievement progress api name", ToolTip = "Shows the Steam progress toast for the achievement with this API name. Max Value 0 uses the Progress Max of its App Definition row. Throttled, and never shown at or above the maximum."))
	static FSteamResult IndicateSteamAchievementProgressByName(const UObject* WorldContextObject, FName AchievementName, int32 CurrentValue, int32 MaxValue = 0);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Steam Achievement Global Percent By Name", Keywords = "get steam achievement global percent percentage api name", ToolTip = "Percentage of all players that unlocked the achievement with this API name (0-100). Returns false until the Request Steam Achievement Percentages node succeeded."))
	static bool GetSteamAchievementGlobalPercentByName(const UObject* WorldContextObject, FName AchievementName, float& Percent);
};
