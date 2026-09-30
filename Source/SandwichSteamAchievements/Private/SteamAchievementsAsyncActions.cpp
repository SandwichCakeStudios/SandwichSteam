// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAchievementsAsyncActions.h"
#include "Core/SteamNameWarnings.h"
#include "SteamAchievementsSubsystem.h"

USteamGetAchievementIconAsyncAction* USteamGetAchievementIconAsyncAction::GetSteamAchievementIcon(const UObject* WorldContextObject, FGameplayTag AchievementTag)
{
	USteamGetAchievementIconAsyncAction* Action = NewObject<USteamGetAchievementIconAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->AchievementTag = AchievementTag;
	return Action;
}

USteamGetAchievementIconAsyncAction* USteamGetAchievementIconAsyncAction::GetSteamAchievementIconByName(const UObject* WorldContextObject, FName AchievementName)
{
	USteamGetAchievementIconAsyncAction* Action = NewObject<USteamGetAchievementIconAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->AchievementName = AchievementName;
	Action->bByName = true;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamGetAchievementIconAsyncAction::GetFeatureClass() const
{
	return USteamAchievementsSubsystem::StaticClass();
}

void USteamGetAchievementIconAsyncAction::StartRequest()
{
	USteamAchievementsSubsystem* Achievements = Cast<USteamAchievementsSubsystem>(GetFeature());
	if (!Achievements)
	{
		return;
	}

	FSteamAchievementIconDelegate OnIcon = FSteamAchievementIconDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, UTexture2D* Texture)
	{
		if (Result.IsSuccess())
		{
			Icon = Texture;
			FinishSuccess();
		}
		else
		{
			FinishFailure(Result);
		}
	});

	const FSteamResult Started = bByName
		? SandwichSteam::WarnOnceIfInvalid(TEXT("achievement"), AchievementName.ToString(), Achievements->RequestIcon(AchievementName, MoveTemp(OnIcon)))
		: Achievements->RequestIcon(AchievementTag, MoveTemp(OnIcon));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamGetAchievementIconAsyncAction::BroadcastSuccess()
{
	OnIconLoaded.Broadcast(Icon);
	Super::BroadcastSuccess();
}

USteamRequestAchievementPercentagesAsyncAction* USteamRequestAchievementPercentagesAsyncAction::RequestSteamAchievementPercentages(const UObject* WorldContextObject)
{
	USteamRequestAchievementPercentagesAsyncAction* Action = NewObject<USteamRequestAchievementPercentagesAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamRequestAchievementPercentagesAsyncAction::GetFeatureClass() const
{
	return USteamAchievementsSubsystem::StaticClass();
}

void USteamRequestAchievementPercentagesAsyncAction::StartRequest()
{
	USteamAchievementsSubsystem* Achievements = Cast<USteamAchievementsSubsystem>(GetFeature());
	if (!Achievements)
	{
		return;
	}

	const FSteamResult Started = Achievements->RequestGlobalPercentages(
		FSteamAchievementPercentagesDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result)
		{
			if (Result.IsSuccess())
			{
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}
