// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamAsyncActionBase.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void USteamAsyncActionBase::Activate()
{
	check(IsInGameThread());

	const UWorld* World = (GEngine && WorldContext.IsValid())
		? GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* OwningGameInstance = World ? World->GetGameInstance() : nullptr;

	if (!OwningGameInstance)
	{
		FinishFailure(FSteamResult::Failure(SteamGameplayTags::Error_Unavailable,
			NSLOCTEXT("SandwichSteam", "AsyncNoGameInstance", "No game instance for this Steam request. Check the World Context.")));
		return;
	}

	GameInstance = OwningGameInstance;
	RegisterWithGameInstance(OwningGameInstance);

	const TSubclassOf<USteamFeatureSubsystem> FeatureClass = GetFeatureClass();
	USteamFeatureSubsystem* FeatureSubsystem = FeatureClass
		? Cast<USteamFeatureSubsystem>(OwningGameInstance->GetSubsystemBase(FeatureClass))
		: nullptr;

	if (!FeatureSubsystem)
	{
		FinishFailure(FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled,
			NSLOCTEXT("SandwichSteam", "AsyncFeatureMissing", "The Steam feature for this request is disabled or not available on this platform.")));
		return;
	}

	FSteamResult InactiveResult;
	if (!FeatureSubsystem->RequireActive(InactiveResult))
	{
		FinishFailure(InactiveResult);
		return;
	}

	Feature = FeatureSubsystem;
	FeatureSubsystem->OnFeatureActiveChanged.AddDynamic(this, &USteamAsyncActionBase::HandleFeatureActiveChanged);

	if (TimeoutSeconds > 0.0f)
	{
		TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float)
			{
				TimeoutHandle.Reset();
				FinishFailure(FSteamResult::Failure(SteamGameplayTags::Error_Timeout,
					NSLOCTEXT("SandwichSteam", "AsyncTimeout", "The Steam request timed out.")));
				return false;
			}),
			TimeoutSeconds);
	}

	StartRequest();
}

void USteamAsyncActionBase::FinishSuccess()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	Cleanup();
	BroadcastSuccess();
	SetReadyToDestroy();
}

void USteamAsyncActionBase::FinishFailure(const FSteamResult& Result)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	Cleanup();
	BroadcastFailure(Result);
	SetReadyToDestroy();
}

void USteamAsyncActionBase::HandleFeatureActiveChanged(bool bActive)
{
	if (!bActive)
	{
		FinishFailure(FSteamResult::Failure(SteamGameplayTags::Error_Cancelled,
			NSLOCTEXT("SandwichSteam", "AsyncCancelled", "The Steam request was cancelled because the Steam feature shut down.")));
	}
}

void USteamAsyncActionBase::Cleanup()
{
	if (TimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
		TimeoutHandle.Reset();
	}

	if (USteamFeatureSubsystem* FeatureSubsystem = Feature.Get())
	{
		FeatureSubsystem->OnFeatureActiveChanged.RemoveDynamic(this, &USteamAsyncActionBase::HandleFeatureActiveChanged);
	}
	Feature.Reset();
}
