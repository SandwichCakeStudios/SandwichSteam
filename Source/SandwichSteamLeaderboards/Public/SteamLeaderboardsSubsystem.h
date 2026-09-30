// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Data/SteamAppDefinition.h"
#include "SteamLeaderboardTypes.h"
#include "SteamLeaderboardsSubsystem.generated.h"

class FSteamLeaderboardsBackend;
struct FSteamRequestKey;

/**
 * Steam leaderboards (ISteamUserStats). Client only.
 *
 * Leaderboards are addressed by gameplay tag (rows of the Steam App Definition, with Create If Missing for boards that
 * are made by the game) or directly by name. Handles are looked up once and cached. Identical downloads that are in
 * flight at the same time share one Steam call. Writing to a leaderboard that is marked "trusted" in Steamworks is not
 * possible from the game client and fails with Steam.Error.NotSupported.
 *
 * Completion delegates run on the game thread. They are not called when the feature shuts down first (the Blueprint
 * async nodes report Steam.Error.Cancelled instead).
 * Do not mix with the engine's OSS leaderboard interface for the same boards.
 */
UCLASS()
class SANDWICHSTEAMLEADERBOARDS_API USteamLeaderboardsSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Leaderboards subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamLeaderboardsSubsystem* Get(const UObject* WorldContext);

	USteamLeaderboardsSubsystem();
	virtual ~USteamLeaderboardsSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/**
	 * Uploads a score. Details (at most 64 values) are stored next to it. The delegate runs once when Steam answered,
	 * possibly not before a later frame. A false Start result means the delegate will never run.
	 */
	FSteamResult UploadScore(const FGameplayTag& LeaderboardTag, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details, FSteamLeaderboardUploadDelegate OnComplete);
	FSteamResult UploadScore(FName LeaderboardName, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details, FSteamLeaderboardUploadDelegate OnComplete);

	/**
	 * Downloads entries. The query is normalized first (range limits, at most 100 entries). Requests that are identical to
	 * one that is already in flight join it: one Steam call, every caller gets the entries.
	 * A failure result on the delegate comes with an empty entry list.
	 */
	FSteamResult DownloadEntries(const FGameplayTag& LeaderboardTag, const FSteamLeaderboardQuery& Query, FSteamLeaderboardDownloadDelegate OnComplete);
	FSteamResult DownloadEntries(FName LeaderboardName, const FSteamLeaderboardQuery& Query, FSteamLeaderboardDownloadDelegate OnComplete);

	/** The App Definition the tags are resolved with. Null when none is assigned (then only names work). */
	const USteamAppDefinition* GetDefinition() const { return Definition; }

	/** Number of downloads that were sent to Steam / that joined an identical download in flight, since the feature became active. */
	int32 GetDownloadCallCount() const { return DownloadCalls; }
	int32 GetJoinedDownloadCount() const { return JoinedDownloads; }

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Leaderboards.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	struct FRequests;

	FSteamResult ResolveTag(const FGameplayTag& LeaderboardTag, FName& OutName) const;

	/** Finds the handle of a leaderboard, from the cache or from Steam. OnResolved runs on the game thread, possibly before this returns. */
	void ResolveHandle(FName Name, TFunction<void(const FSteamResult&, uint64)> OnResolved);
	void CompleteHandle(FName Name, const FSteamResult& Result, uint64 Handle);

	void StartUpload(uint64 Handle, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details, const FSteamLeaderboardUploadDelegate& OnComplete);
	void StartDownload(uint64 Handle, const FSteamRequestKey& Key, const FSteamLeaderboardQuery& Query);
	void CompleteDownload(const FSteamRequestKey& Key, const FSteamResult& Result, const TArray<FSteamLeaderboardEntry>& Entries);

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	TSharedPtr<FSteamLeaderboardsBackend> Backend;
	TSharedPtr<FRequests> Requests;

	/** Leaderboard name -> Steam handle. Bounded by the number of leaderboards the game uses. */
	TMap<FName, uint64> Handles;

	int32 DownloadCalls = 0;
	int32 JoinedDownloads = 0;
};
