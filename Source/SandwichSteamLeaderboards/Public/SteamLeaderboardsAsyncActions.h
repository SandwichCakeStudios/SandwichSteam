// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "SteamLeaderboardTypes.h"
#include "SteamLeaderboardsAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamLeaderboardUploadedAsyncDelegate, const FSteamLeaderboardUploadResult&, Upload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamLeaderboardEntriesAsyncDelegate, const TArray<FSteamLeaderboardEntry>&, Entries);

/** Uploads a score to a leaderboard of the Steam App Definition and reports the rank change. */
UCLASS()
class SANDWICHSTEAMLEADERBOARDS_API USteamUploadLeaderboardScoreAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with what Steam answered. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Leaderboards", meta = (ToolTip = "Called with the score, whether it changed and the rank before and after."))
	FSteamLeaderboardUploadedAsyncDelegate OnScoreUploaded;

	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Leaderboard", AutoCreateRefTerm = "Details", DisplayName = "Upload Steam Leaderboard Score", ToolTip = "Uploads a score. Details (at most 64 values) are stored next to it. Fails with Not Supported for leaderboards marked trusted in Steamworks."))
	static USteamUploadLeaderboardScoreAsyncAction* UploadSteamLeaderboardScore(const UObject* WorldContextObject, FGameplayTag LeaderboardTag, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details);

	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards|By Name", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Details", DisplayName = "Upload Steam Leaderboard Score By Name", Keywords = "upload submit steam leaderboard score name", ToolTip = "Uploads a score to the leaderboard with this name in Steamworks. Details (at most 64 values) are stored next to it. The board must exist, unless its App Definition row has Create If Missing."))
	static USteamUploadLeaderboardScoreAsyncAction* UploadSteamLeaderboardScoreByName(const UObject* WorldContextObject, FName LeaderboardName, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FGameplayTag LeaderboardTag;
	FName LeaderboardName;
	bool bByName = false;
	int32 Score = 0;
	ESteamLeaderboardUploadMethod Method = ESteamLeaderboardUploadMethod::KeepBest;
	TArray<int32> Details;
	FSteamLeaderboardUploadResult Upload;
};

/** Downloads entries of a leaderboard of the Steam App Definition: top ranks, around the local user, friends or listed users. */
UCLASS()
class SANDWICHSTEAMLEADERBOARDS_API USteamDownloadLeaderboardEntriesAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the entries, in the order Steam returns them. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Leaderboards", meta = (ToolTip = "Called with the downloaded entries."))
	FSteamLeaderboardEntriesAsyncDelegate OnEntriesLoaded;

	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Leaderboard", DisplayName = "Download Steam Leaderboard Entries", ToolTip = "Downloads entries of a leaderboard (at most 100). Identical requests made at the same time share one Steam call."))
	static USteamDownloadLeaderboardEntriesAsyncAction* DownloadSteamLeaderboardEntries(const UObject* WorldContextObject, FGameplayTag LeaderboardTag, const FSteamLeaderboardQuery& Query);

	UFUNCTION(BlueprintCallable, Category = "Steam|Leaderboards|By Name", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Download Steam Leaderboard Entries By Name", Keywords = "download get read steam leaderboard entries scores name", ToolTip = "Downloads entries (at most 100) of the leaderboard with this name in Steamworks. Identical requests made at the same time share one Steam call."))
	static USteamDownloadLeaderboardEntriesAsyncAction* DownloadSteamLeaderboardEntriesByName(const UObject* WorldContextObject, FName LeaderboardName, const FSteamLeaderboardQuery& Query);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FGameplayTag LeaderboardTag;
	FName LeaderboardName;
	bool bByName = false;
	FSteamLeaderboardQuery Query;
	TArray<FSteamLeaderboardEntry> Entries;
};
