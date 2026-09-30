// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "SteamStatsAsyncActions.generated.h"

/** Waits until Steam delivered the stats of the local user. Finishes at once when they are already there. */
UCLASS()
class SANDWICHSTEAMSTATS_API USteamWaitForStatsAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Wait For Steam Stats", ToolTip = "Waits until Steam delivered the stats of the local user. Finishes at once when they are already there."))
	static USteamWaitForStatsAsyncAction* WaitForSteamStats(const UObject* WorldContextObject);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
};

/** Uploads changed stats to Steam and waits for the result. */
UCLASS()
class SANDWICHSTEAMSTATS_API USteamStoreStatsAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Stats", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Store Steam Stats", ToolTip = "Uploads changed stats to Steam right now and waits for the result. Finishes at once when nothing changed."))
	static USteamStoreStatsAsyncAction* StoreSteamStats(const UObject* WorldContextObject);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
};
