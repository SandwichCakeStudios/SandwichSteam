// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Data/SteamAppDefinition.h"
#include "SteamLeaderboardTypes.h"

/**
 * Raw ISteamUserStats leaderboard access. The header has no Steamworks types. Every completion runs on the game thread
 * (through TSteamCallResult and the dispatcher) and never after the backend was destroyed: destroying the backend cancels
 * everything in flight. Leaderboard handles are plain uint64 (SteamLeaderboard_t).
 *
 * Downloads run one at a time: Steam invalidates the entries of a download when the next one starts, and the entries
 * are read when the result arrives on the game thread, so a queued job only starts after the previous one was read.
 * Call every method on the game thread, only while Steam is Ready.
 */
class FSteamLeaderboardsBackend
{
public:
	/** bIOFailure: Steam did not answer. bFound: the leaderboard exists. Handle: valid when bFound. */
	using FFindDone = TFunction<void(bool /*bIOFailure*/, bool /*bFound*/, uint64 /*Handle*/)>;

	/** bIOFailure: Steam did not answer. bAccepted: Steam stored or evaluated the score (false for e.g. trusted leaderboards). */
	using FUploadDone = TFunction<void(bool /*bIOFailure*/, bool /*bAccepted*/, const FSteamLeaderboardUploadResult& /*Result*/)>;

	using FDownloadDone = TFunction<void(bool /*bSuccess*/, const TArray<FSteamLeaderboardEntry>& /*Entries*/)>;

	explicit FSteamLeaderboardsBackend(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);
	~FSteamLeaderboardsBackend();

	FSteamLeaderboardsBackend(const FSteamLeaderboardsBackend&) = delete;
	FSteamLeaderboardsBackend& operator=(const FSteamLeaderboardsBackend&) = delete;

	/** Looks a leaderboard up by name. Returns false when the request could not be started (OnDone is not called then). */
	bool FindLeaderboard(FName Name, FFindDone OnDone);

	/** Looks a leaderboard up and creates it with the given sort and display type when it does not exist. */
	bool FindOrCreateLeaderboard(FName Name, ESteamLeaderboardSortMethod SortMethod, ESteamLeaderboardDisplayType DisplayType, FFindDone OnDone);

	bool UploadScore(uint64 Handle, ESteamLeaderboardUploadMethod Method, int32 Score, TConstArrayView<int32> Details, FUploadDone OnDone);

	/** Downloads by a normalized query (SandwichSteam::Leaderboards::NormalizeQuery). */
	bool DownloadEntries(uint64 Handle, const FSteamLeaderboardQuery& NormalizedQuery, FDownloadDone OnDone);

private:
	struct FImpl;

	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
	TUniquePtr<FImpl> Impl;
};
