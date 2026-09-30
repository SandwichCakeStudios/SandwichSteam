// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamLeaderboardsAsyncActions.h"
#include "SteamLeaderboardsSubsystem.h"

USteamUploadLeaderboardScoreAsyncAction* USteamUploadLeaderboardScoreAsyncAction::UploadSteamLeaderboardScore(const UObject* WorldContextObject, FGameplayTag LeaderboardTag, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details)
{
	USteamUploadLeaderboardScoreAsyncAction* Action = NewObject<USteamUploadLeaderboardScoreAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->LeaderboardTag = LeaderboardTag;
	Action->Score = Score;
	Action->Method = Method;
	Action->Details = Details;
	return Action;
}

USteamUploadLeaderboardScoreAsyncAction* USteamUploadLeaderboardScoreAsyncAction::UploadSteamLeaderboardScoreByName(const UObject* WorldContextObject, FName LeaderboardName, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details)
{
	USteamUploadLeaderboardScoreAsyncAction* Action = NewObject<USteamUploadLeaderboardScoreAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->LeaderboardName = LeaderboardName;
	Action->bByName = true;
	Action->Score = Score;
	Action->Method = Method;
	Action->Details = Details;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamUploadLeaderboardScoreAsyncAction::GetFeatureClass() const
{
	return USteamLeaderboardsSubsystem::StaticClass();
}

void USteamUploadLeaderboardScoreAsyncAction::StartRequest()
{
	USteamLeaderboardsSubsystem* Leaderboards = Cast<USteamLeaderboardsSubsystem>(GetFeature());
	if (!Leaderboards)
	{
		return;
	}

	// An unknown board name is already logged by the subsystem's handle lookup, so no extra warning here.
	FSteamLeaderboardUploadDelegate OnUploaded = FSteamLeaderboardUploadDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const FSteamLeaderboardUploadResult& Uploaded)
	{
		if (Result.IsSuccess())
		{
			Upload = Uploaded;
			FinishSuccess();
		}
		else
		{
			FinishFailure(Result);
		}
	});

	const FSteamResult Started = bByName
		? Leaderboards->UploadScore(LeaderboardName, Score, Method, Details, MoveTemp(OnUploaded))
		: Leaderboards->UploadScore(LeaderboardTag, Score, Method, Details, MoveTemp(OnUploaded));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamUploadLeaderboardScoreAsyncAction::BroadcastSuccess()
{
	OnScoreUploaded.Broadcast(Upload);
	Super::BroadcastSuccess();
}

USteamDownloadLeaderboardEntriesAsyncAction* USteamDownloadLeaderboardEntriesAsyncAction::DownloadSteamLeaderboardEntries(const UObject* WorldContextObject, FGameplayTag LeaderboardTag, const FSteamLeaderboardQuery& Query)
{
	USteamDownloadLeaderboardEntriesAsyncAction* Action = NewObject<USteamDownloadLeaderboardEntriesAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->LeaderboardTag = LeaderboardTag;
	Action->Query = Query;
	return Action;
}

USteamDownloadLeaderboardEntriesAsyncAction* USteamDownloadLeaderboardEntriesAsyncAction::DownloadSteamLeaderboardEntriesByName(const UObject* WorldContextObject, FName LeaderboardName, const FSteamLeaderboardQuery& Query)
{
	USteamDownloadLeaderboardEntriesAsyncAction* Action = NewObject<USteamDownloadLeaderboardEntriesAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->LeaderboardName = LeaderboardName;
	Action->bByName = true;
	Action->Query = Query;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamDownloadLeaderboardEntriesAsyncAction::GetFeatureClass() const
{
	return USteamLeaderboardsSubsystem::StaticClass();
}

void USteamDownloadLeaderboardEntriesAsyncAction::StartRequest()
{
	USteamLeaderboardsSubsystem* Leaderboards = Cast<USteamLeaderboardsSubsystem>(GetFeature());
	if (!Leaderboards)
	{
		return;
	}

	FSteamLeaderboardDownloadDelegate OnLoaded = FSteamLeaderboardDownloadDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const TArray<FSteamLeaderboardEntry>& Loaded)
	{
		if (Result.IsSuccess())
		{
			Entries = Loaded;
			FinishSuccess();
		}
		else
		{
			FinishFailure(Result);
		}
	});

	const FSteamResult Started = bByName
		? Leaderboards->DownloadEntries(LeaderboardName, Query, MoveTemp(OnLoaded))
		: Leaderboards->DownloadEntries(LeaderboardTag, Query, MoveTemp(OnLoaded));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamDownloadLeaderboardEntriesAsyncAction::BroadcastSuccess()
{
	OnEntriesLoaded.Broadcast(Entries);
	Super::BroadcastSuccess();
}
