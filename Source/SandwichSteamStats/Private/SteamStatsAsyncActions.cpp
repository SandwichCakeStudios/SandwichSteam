// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamStatsAsyncActions.h"
#include "SteamStatsSubsystem.h"

USteamWaitForStatsAsyncAction* USteamWaitForStatsAsyncAction::WaitForSteamStats(const UObject* WorldContextObject)
{
	USteamWaitForStatsAsyncAction* Action = NewObject<USteamWaitForStatsAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamWaitForStatsAsyncAction::GetFeatureClass() const
{
	return USteamStatsSubsystem::StaticClass();
}

void USteamWaitForStatsAsyncAction::StartRequest()
{
	USteamStatsSubsystem* Stats = Cast<USteamStatsSubsystem>(GetFeature());
	if (!Stats)
	{
		return;
	}

	if (Stats->AreStatsReady())
	{
		FinishSuccess();
		return;
	}

	// The weak lambda is skipped once this action is destroyed. FinishSuccess is a no-op when already finished.
	Stats->OnStatsReadyNative.AddWeakLambda(this, [this]()
	{
		FinishSuccess();
	});
}

USteamStoreStatsAsyncAction* USteamStoreStatsAsyncAction::StoreSteamStats(const UObject* WorldContextObject)
{
	USteamStoreStatsAsyncAction* Action = NewObject<USteamStoreStatsAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamStoreStatsAsyncAction::GetFeatureClass() const
{
	return USteamStatsSubsystem::StaticClass();
}

void USteamStoreStatsAsyncAction::StartRequest()
{
	USteamStatsSubsystem* Stats = Cast<USteamStatsSubsystem>(GetFeature());
	if (!Stats)
	{
		return;
	}

	const FSteamResult Started = Stats->StoreStatsNow();
	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
		return;
	}

	if (!Stats->IsStorePending())
	{
		FinishSuccess(); // Nothing changed.
		return;
	}

	Stats->OnStatsStoredNative.AddWeakLambda(this, [this](const FSteamResult& Result)
	{
		if (Result.IsSuccess())
		{
			FinishSuccess();
		}
		else
		{
			FinishFailure(Result);
		}
	});
}
