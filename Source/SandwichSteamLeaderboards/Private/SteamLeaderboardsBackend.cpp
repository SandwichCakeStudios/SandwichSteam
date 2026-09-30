// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamLeaderboardsBackend.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

#include "Core/SteamCallResult.h"
#include "Core/SteamSDK.h"

namespace
{
	FTCHARToUTF8 ToUtf8(FName Name)
	{
		return FTCHARToUTF8(*Name.ToString());
	}

	ELeaderboardSortMethod ToSteam(ESteamLeaderboardSortMethod Method)
	{
		return Method == ESteamLeaderboardSortMethod::Ascending ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending;
	}

	ELeaderboardDisplayType ToSteam(ESteamLeaderboardDisplayType Type)
	{
		switch (Type)
		{
		case ESteamLeaderboardDisplayType::TimeSeconds:
			return k_ELeaderboardDisplayTypeTimeSeconds;
		case ESteamLeaderboardDisplayType::TimeMilliSeconds:
			return k_ELeaderboardDisplayTypeTimeMilliSeconds;
		default:
			return k_ELeaderboardDisplayTypeNumeric;
		}
	}

	ELeaderboardDataRequest ToSteam(ESteamLeaderboardRequestType Type)
	{
		switch (Type)
		{
		case ESteamLeaderboardRequestType::AroundUser:
			return k_ELeaderboardDataRequestGlobalAroundUser;
		case ESteamLeaderboardRequestType::Friends:
			return k_ELeaderboardDataRequestFriends;
		case ESteamLeaderboardRequestType::Users:
			return k_ELeaderboardDataRequestUsers;
		default:
			return k_ELeaderboardDataRequestGlobal;
		}
	}
}

/** Everything that needs the Steamworks SDK: the pending call results and the download queue. */
struct FSteamLeaderboardsBackend::FImpl
{
	struct FDownloadJob
	{
		uint64 Handle = 0;
		FSteamLeaderboardQuery Query;
		FDownloadDone OnDone;
	};

	explicit FImpl(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
		: Dispatcher(InDispatcher)
	{
	}

	bool StartFind(SteamAPICall_t Call, FFindDone OnDone)
	{
		return TSteamCallResult<LeaderboardFindResult_t>::Start(PendingFind, Dispatcher, Call,
			[OnDone = MoveTemp(OnDone)](const LeaderboardFindResult_t& Result, bool bIOFailure)
			{
				OnDone(bIOFailure, !bIOFailure && Result.m_bLeaderboardFound != 0, Result.m_hSteamLeaderboard);
			}) != nullptr;
	}

	/** Starts queued downloads until one is in flight. Runs on the game thread. */
	void PumpDownloads()
	{
		ISteamUserStats* Stats = SteamUserStats();
		while (!bDownloadBusy && !Jobs.IsEmpty())
		{
			FDownloadJob Job = MoveTemp(Jobs[0]);
			Jobs.RemoveAt(0);

			SteamAPICall_t Call = k_uAPICallInvalid;
			if (Stats)
			{
				if (Job.Query.Type == ESteamLeaderboardRequestType::Users)
				{
					TArray<CSteamID> Users;
					Users.Reserve(Job.Query.Users.Num());
					for (const FSteamId& User : Job.Query.Users)
					{
						Users.Add(CSteamID(static_cast<uint64>(User.Value)));
					}
					Call = Stats->DownloadLeaderboardEntriesForUsers(Job.Handle, Users.GetData(), Users.Num());
				}
				else
				{
					Call = Stats->DownloadLeaderboardEntries(Job.Handle, ToSteam(Job.Query.Type), Job.Query.RangeStart, Job.Query.RangeEnd);
				}
			}

			FDownloadDone OnDone = Job.OnDone;
			const bool bStarted = TSteamCallResult<LeaderboardScoresDownloaded_t>::Start(PendingDownload, Dispatcher, Call,
				[this, OnDone](const LeaderboardScoresDownloaded_t& Result, bool bIOFailure)
				{
					// Read the entries now: they are only valid until the next download starts.
					TArray<FSteamLeaderboardEntry> Entries;
					if (!bIOFailure)
					{
						ReadEntries(Result, Entries);
					}

					bDownloadBusy = false;
					OnDone(!bIOFailure, Entries);
					PumpDownloads();
				}) != nullptr;

			if (bStarted)
			{
				bDownloadBusy = true;
			}
			else
			{
				OnDone(false, TArray<FSteamLeaderboardEntry>());
			}
		}
	}

	static void ReadEntries(const LeaderboardScoresDownloaded_t& Result, TArray<FSteamLeaderboardEntry>& OutEntries)
	{
		ISteamUserStats* Stats = SteamUserStats();
		if (!Stats)
		{
			return;
		}

		const int32 Count = FMath::Clamp(static_cast<int32>(Result.m_cEntryCount), 0, SandwichSteam::Leaderboards::MaxPageSize);
		OutEntries.Reserve(Count);

		int32 Details[SandwichSteam::Leaderboards::MaxDetails];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			LeaderboardEntry_t Entry;
			if (!Stats->GetDownloadedLeaderboardEntry(Result.m_hSteamLeaderboardEntries, Index, &Entry, Details, SandwichSteam::Leaderboards::MaxDetails))
			{
				continue;
			}

			FSteamLeaderboardEntry& Out = OutEntries.AddDefaulted_GetRef();
			Out.SteamId = FSteamId(static_cast<int64>(Entry.m_steamIDUser.ConvertToUint64()));
			Out.Rank = Entry.m_nGlobalRank;
			Out.Score = Entry.m_nScore;
			Out.Details.Append(Details, FMath::Clamp(static_cast<int32>(Entry.m_cDetails), 0, SandwichSteam::Leaderboards::MaxDetails));
		}
	}

	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	TArray<FDownloadJob> Jobs;
	bool bDownloadBusy = false;

	TSteamCallResult<LeaderboardFindResult_t>::FPendingList PendingFind;
	TSteamCallResult<LeaderboardScoreUploaded_t>::FPendingList PendingUpload;
	TSteamCallResult<LeaderboardScoresDownloaded_t>::FPendingList PendingDownload;
};

FSteamLeaderboardsBackend::FSteamLeaderboardsBackend(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Dispatcher(InDispatcher)
	, Impl(MakeUnique<FImpl>(InDispatcher))
{
}

FSteamLeaderboardsBackend::~FSteamLeaderboardsBackend() = default;

bool FSteamLeaderboardsBackend::FindLeaderboard(FName Name, FFindDone OnDone)
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Impl->StartFind(Stats->FindLeaderboard(ToUtf8(Name).Get()), MoveTemp(OnDone));
}

bool FSteamLeaderboardsBackend::FindOrCreateLeaderboard(FName Name, ESteamLeaderboardSortMethod SortMethod, ESteamLeaderboardDisplayType DisplayType, FFindDone OnDone)
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Impl->StartFind(Stats->FindOrCreateLeaderboard(ToUtf8(Name).Get(), ToSteam(SortMethod), ToSteam(DisplayType)), MoveTemp(OnDone));
}

bool FSteamLeaderboardsBackend::UploadScore(uint64 Handle, ESteamLeaderboardUploadMethod Method, int32 Score, TConstArrayView<int32> Details, FUploadDone OnDone)
{
	ISteamUserStats* Stats = SteamUserStats();
	if (!Stats)
	{
		return false;
	}

	const ELeaderboardUploadScoreMethod SteamMethod = Method == ESteamLeaderboardUploadMethod::ForceUpdate
		? k_ELeaderboardUploadScoreMethodForceUpdate
		: k_ELeaderboardUploadScoreMethodKeepBest;

	const int32 DetailCount = FMath::Min(Details.Num(), SandwichSteam::Leaderboards::MaxDetails);
	const SteamAPICall_t Call = Stats->UploadLeaderboardScore(Handle, SteamMethod, Score, DetailCount > 0 ? Details.GetData() : nullptr, DetailCount);

	return TSteamCallResult<LeaderboardScoreUploaded_t>::Start(Impl->PendingUpload, Dispatcher, Call,
		[OnDone = MoveTemp(OnDone)](const LeaderboardScoreUploaded_t& Result, bool bIOFailure)
		{
			FSteamLeaderboardUploadResult Upload;
			Upload.Score = Result.m_nScore;
			Upload.bScoreChanged = Result.m_bScoreChanged != 0;
			Upload.NewRank = Result.m_nGlobalRankNew;
			Upload.PreviousRank = Result.m_nGlobalRankPrevious;
			OnDone(bIOFailure, !bIOFailure && Result.m_bSuccess != 0, Upload);
		}) != nullptr;
}

bool FSteamLeaderboardsBackend::DownloadEntries(uint64 Handle, const FSteamLeaderboardQuery& NormalizedQuery, FDownloadDone OnDone)
{
	if (!SteamUserStats())
	{
		return false;
	}

	FImpl::FDownloadJob& Job = Impl->Jobs.AddDefaulted_GetRef();
	Job.Handle = Handle;
	Job.Query = NormalizedQuery;
	Job.OnDone = MoveTemp(OnDone);
	Impl->PumpDownloads();
	return true;
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

struct FSteamLeaderboardsBackend::FImpl {};

FSteamLeaderboardsBackend::FSteamLeaderboardsBackend(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Dispatcher(InDispatcher)
{
}

FSteamLeaderboardsBackend::~FSteamLeaderboardsBackend() = default;
bool FSteamLeaderboardsBackend::FindLeaderboard(FName, FFindDone) { return false; }
bool FSteamLeaderboardsBackend::FindOrCreateLeaderboard(FName, ESteamLeaderboardSortMethod, ESteamLeaderboardDisplayType, FFindDone) { return false; }
bool FSteamLeaderboardsBackend::UploadScore(uint64, ESteamLeaderboardUploadMethod, int32, TConstArrayView<int32>, FUploadDone) { return false; }
bool FSteamLeaderboardsBackend::DownloadEntries(uint64, const FSteamLeaderboardQuery&, FDownloadDone) { return false; }

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
