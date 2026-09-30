// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "SteamAchievementsAsyncActions.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAchievementIconAsyncDelegate, UTexture2D*, Icon);

/** Loads the icon of an achievement (the locked or unlocked version, whichever is current). Cached afterwards. */
UCLASS()
class SANDWICHSTEAMACHIEVEMENTS_API USteamGetAchievementIconAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the icon. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Achievements", meta = (ToolTip = "Called with the achievement icon once it is loaded."))
	FSteamAchievementIconAsyncDelegate OnIconLoaded;

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Achievement", DisplayName = "Get Steam Achievement Icon", ToolTip = "Loads the icon of an achievement (locked or unlocked version, whichever is current)."))
	static USteamGetAchievementIconAsyncAction* GetSteamAchievementIcon(const UObject* WorldContextObject, FGameplayTag AchievementTag);

	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements|By Name", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Get Steam Achievement Icon By Name", Keywords = "get steam achievement icon image api name", ToolTip = "Loads the icon of the achievement with this API name in Steamworks (locked or unlocked version, whichever is current)."))
	static USteamGetAchievementIconAsyncAction* GetSteamAchievementIconByName(const UObject* WorldContextObject, FName AchievementName);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon;

	FGameplayTag AchievementTag;
	FName AchievementName;
	bool bByName = false;
};

/** Asks Steam how many players unlocked each achievement. Read the values with Get Steam Achievement Global Percent. */
UCLASS()
class SANDWICHSTEAMACHIEVEMENTS_API USteamRequestAchievementPercentagesAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Achievements", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Request Steam Achievement Percentages", ToolTip = "Asks Steam how many players unlocked each achievement. Afterwards read the values with Get Steam Achievement Global Percent."))
	static USteamRequestAchievementPercentagesAsyncAction* RequestSteamAchievementPercentages(const UObject* WorldContextObject);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
};
