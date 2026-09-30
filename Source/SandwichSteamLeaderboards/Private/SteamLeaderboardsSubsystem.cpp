// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamLeaderboardsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SteamLeaderboardsBackend.h"
#include "SteamRequestKey.h"

namespace
{
	using FHandleWaiter = TFunction<void(const FSteamResult&, uint64)>;
}

/** In-flight requests, hidden from the public header. */
struct USteamLeaderboardsSubsystem::FRequests
{
	TSteamRequestCoalescer<FName, FHandleWaiter> Handles;
	TSteamRequestCoalescer<FSteamRequestKey, FSteamLeaderboardDownloadDelegate> Downloads;
};

USteamLeaderboardsSubsystem* USteamLeaderboardsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamLeaderboardsSubsystem>() : nullptr;
}

USteamLeaderboardsSubsystem::USteamLeaderboardsSubsystem() = default;
USteamLeaderboardsSubsystem::~USteamLeaderboardsSubsystem() = default;

FGameplayTag USteamLeaderboardsSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Leaderboards;
}

bool USteamLeaderboardsSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	Definition = ToolSettings ? ToolSettings->LoadAppDefinition() : nullptr;
	if (!Definition)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam leaderboards: no App Definition assigned in the Sandwich Steam settings. Leaderboards work by name only."));
	}

	Backend = MakeShared<FSteamLeaderboardsBackend>(Dispatcher.ToSharedRef());
	Requests = MakeShared<FRequests>();
	DownloadCalls = 0;
	JoinedDownloads = 0;
	return true;
#else
	return false;
#endif
}

void USteamLeaderboardsSubsystem::ShutdownFeature()
{
	// Destroying the backend cancels every Steam call in flight; the waiters are dropped with the requests.
	Backend.Reset();
	Requests.Reset();
	Handles.Reset();
	Definition = nullptr;
}

FSteamResult USteamLeaderboardsSubsystem::ResolveTag(const FGameplayTag& LeaderboardTag, FName& OutName) const
{
	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "LbNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so leaderboards cannot be accessed by tag."));
	}

	const FSteamLeaderboardDef* Def = Definition->FindLeaderboard(LeaderboardTag);
	if (!Def)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "LbUnknownTag", "The tag '{0}' is not a leaderboard of the Steam App Definition."), FText::FromName(LeaderboardTag.GetTagName())));
	}

	OutName = Def->Name;
	return FSteamResult::Success();
}

void USteamLeaderboardsSubsystem::ResolveHandle(FName Name, TFunction<void(const FSteamResult&, uint64)> OnResolved)
{
	if (const uint64* Cached = Handles.Find(Name))
	{
		OnResolved(FSteamResult::Success(), *Cached);
		return;
	}

	if (!Requests->Handles.Join(Name, MoveTemp(OnResolved)))
	{
		return; // A lookup of this name is already running; the waiter is answered with it.
	}

	const TWeakObjectPtr<USteamLeaderboardsSubsystem> WeakThis(this);
	const FSteamLeaderboardsBackend::FFindDone OnFound = [WeakThis, Name](bool bIOFailure, bool bFound, uint64 Handle)
	{
		USteamLeaderboardsSubsystem* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (bFound)
		{
			This->CompleteHandle(Name, FSteamResult::Success(), Handle);
		}
		else if (bIOFailure)
		{
			This->CompleteHandle(Name, FSteamResult::Failure(SteamGameplayTags::Error_Failed,
				FText::Format(NSLOCTEXT("SandwichSteam", "LbFindIo", "Steam did not answer while looking up the leaderboard '{0}'."), FText::FromName(Name))), 0);
		}
		else
		{
			This->CompleteHandle(Name, FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
				FText::Format(NSLOCTEXT("SandwichSteam", "LbNotFound", "Steam does not know the leaderboard '{0}'. Check the name in Steamworks, or enable Create If Missing for it in the App Definition."), FText::FromName(Name))), 0);
		}
	};

	const FSteamLeaderboardDef* Def = Definition ? Definition->FindLeaderboard(Name) : nullptr;
	const bool bStarted = (Def && Def->bCreateIfMissing)
		? Backend->FindOrCreateLeaderboard(Name, Def->SortMethod, Def->DisplayType, OnFound)
		: Backend->FindLeaderboard(Name, OnFound);

	if (!bStarted)
	{
		CompleteHandle(Name, FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			FText::Format(NSLOCTEXT("SandwichSteam", "LbFindNotStarted", "Steam refused to look up the leaderboard '{0}'."), FText::FromName(Name))), 0);
	}
}

void USteamLeaderboardsSubsystem::CompleteHandle(FName Name, const FSteamResult& Result, uint64 Handle)
{
	if (Result.IsSuccess())
	{
		Handles.Add(Name, Handle);
	}
	else
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam leaderboards: %s"), *Result.Message.ToString());
	}

	if (!Requests.IsValid())
	{
		return;
	}

	// Take first: a waiter may start new requests.
	const TArray<FHandleWaiter> Waiters = Requests->Handles.Take(Name);
	for (const FHandleWaiter& Waiter : Waiters)
	{
		Waiter(Result, Handle);
	}
}

FSteamResult USteamLeaderboardsSubsystem::UploadScore(const FGameplayTag& LeaderboardTag, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details, FSteamLeaderboardUploadDelegate OnComplete)
{
	FName Name;
	const FSteamResult Resolved = ResolveTag(LeaderboardTag, Name);
	return Resolved.IsSuccess() ? UploadScore(Name, Score, Method, Details, MoveTemp(OnComplete)) : Resolved;
}

FSteamResult USteamLeaderboardsSubsystem::UploadScore(FName LeaderboardName, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details, FSteamLeaderboardUploadDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (LeaderboardName.IsNone())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "LbEmptyName", "The leaderboard name is empty."));
	}

	if (Details.Num() > SandwichSteam::Leaderboards::MaxDetails)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "LbTooManyDetails", "A score takes at most {0} detail values."), FText::AsNumber(SandwichSteam::Leaderboards::MaxDetails)));
	}

	const TWeakObjectPtr<USteamLeaderboardsSubsystem> WeakThis(this);
	ResolveHandle(LeaderboardName, [WeakThis, Score, Method, Details, OnComplete](const FSteamResult& Resolved, uint64 Handle)
	{
		USteamLeaderboardsSubsystem* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (!Resolved.IsSuccess())
		{
			OnComplete.ExecuteIfBound(Resolved, FSteamLeaderboardUploadResult());
			return;
		}

		This->StartUpload(Handle, Score, Method, Details, OnComplete);
	});

	return FSteamResult::Success();
}

void USteamLeaderboardsSubsystem::StartUpload(uint64 Handle, int32 Score, ESteamLeaderboardUploadMethod Method, const TArray<int32>& Details, const FSteamLeaderboardUploadDelegate& OnComplete)
{
	const bool bStarted = Backend.IsValid() && Backend->UploadScore(Handle, Method, Score, Details,
		[OnComplete](bool bIOFailure, bool bAccepted, const FSteamLeaderboardUploadResult& Upload)
		{
			if (bIOFailure)
			{
				OnComplete.ExecuteIfBound(FSteamResult::Failure(SteamGameplayTags::Error_Failed,
					NSLOCTEXT("SandwichSteam", "LbUploadIo", "Steam did not answer the score upload.")), FSteamLeaderboardUploadResult());
			}
			else if (!bAccepted)
			{
				OnComplete.ExecuteIfBound(FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
					NSLOCTEXT("SandwichSteam", "LbUploadRefused", "Steam did not accept the score. Leaderboards set to trusted in Steamworks only take scores from the game server or the Web API.")), FSteamLeaderboardUploadResult());
			}
			else
			{
				OnComplete.ExecuteIfBound(FSteamResult::Success(), Upload);
			}
		});

	if (!bStarted)
	{
		OnComplete.ExecuteIfBound(FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "LbUploadNotStarted", "Steam refused the score upload.")), FSteamLeaderboardUploadResult());
	}
}

FSteamResult USteamLeaderboardsSubsystem::DownloadEntries(const FGameplayTag& LeaderboardTag, const FSteamLeaderboardQuery& Query, FSteamLeaderboardDownloadDelegate OnComplete)
{
	FName Name;
	const FSteamResult Resolved = ResolveTag(LeaderboardTag, Name);
	return Resolved.IsSuccess() ? DownloadEntries(Name, Query, MoveTemp(OnComplete)) : Resolved;
}

FSteamResult USteamLeaderboardsSubsystem::DownloadEntries(FName LeaderboardName, const FSteamLeaderboardQuery& Query, FSteamLeaderboardDownloadDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (LeaderboardName.IsNone())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "LbEmptyNameDownload", "The leaderboard name is empty."));
	}

	const FSteamLeaderboardQuery Normalized = SandwichSteam::Leaderboards::NormalizeQuery(Query);
	if (Normalized.Type == ESteamLeaderboardRequestType::Users && Normalized.Users.IsEmpty())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "LbNoUsers", "A Users query needs at least one valid Steam ID."));
	}

	const FSteamRequestKey Key = FSteamRequestKey::ForDownload(LeaderboardName, Normalized);
	if (!Requests->Downloads.Join(Key, MoveTemp(OnComplete)))
	{
		++JoinedDownloads;
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam leaderboards: download of '%s' joined an identical request in flight."), *LeaderboardName.ToString());
		return FSteamResult::Success();
	}

	const TWeakObjectPtr<USteamLeaderboardsSubsystem> WeakThis(this);
	ResolveHandle(LeaderboardName, [WeakThis, Key, Normalized](const FSteamResult& Resolved, uint64 Handle)
	{
		USteamLeaderboardsSubsystem* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (Resolved.IsSuccess())
		{
			This->StartDownload(Handle, Key, Normalized);
		}
		else
		{
			This->CompleteDownload(Key, Resolved, TArray<FSteamLeaderboardEntry>());
		}
	});

	return FSteamResult::Success();
}

void USteamLeaderboardsSubsystem::StartDownload(uint64 Handle, const FSteamRequestKey& Key, const FSteamLeaderboardQuery& Query)
{
	++DownloadCalls;

	const TWeakObjectPtr<USteamLeaderboardsSubsystem> WeakThis(this);
	const bool bStarted = Backend.IsValid() && Backend->DownloadEntries(Handle, Query,
		[WeakThis, Key](bool bSuccess, const TArray<FSteamLeaderboardEntry>& Entries)
		{
			USteamLeaderboardsSubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}

			This->CompleteDownload(Key, bSuccess
				? FSteamResult::Success()
				: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LbDownloadFailed", "Steam could not download the leaderboard entries.")), Entries);
		});

	if (!bStarted)
	{
		CompleteDownload(Key, FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LbDownloadNotStarted", "Steam refused the leaderboard download.")), TArray<FSteamLeaderboardEntry>());
	}
}

void USteamLeaderboardsSubsystem::CompleteDownload(const FSteamRequestKey& Key, const FSteamResult& Result, const TArray<FSteamLeaderboardEntry>& Entries)
{
	if (!Requests.IsValid())
	{
		return;
	}

	// Take first: a waiter may start new requests, including an identical one.
	const TArray<FSteamLeaderboardDownloadDelegate> Waiters = Requests->Downloads.Take(Key);
	for (const FSteamLeaderboardDownloadDelegate& Waiter : Waiters)
	{
		Waiter.ExecuteIfBound(Result, Entries);
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamLeaderboardsSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Leaderboards: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	Report += FString::Printf(TEXT("  Definition: %s (%d leaderboards)\n"), Definition ? *Definition->GetName() : TEXT("none"), Definition ? Definition->Leaderboards.Num() : 0);
	Report += FString::Printf(TEXT("  Downloads: %d sent to Steam, %d joined an identical one in flight\n"), DownloadCalls, JoinedDownloads);
	if (Requests.IsValid())
	{
		Report += FString::Printf(TEXT("  In flight: %d lookups, %d downloads\n"), Requests->Handles.Num(), Requests->Downloads.Num());
	}

	if (Definition)
	{
		for (const FSteamLeaderboardDef& Def : Definition->Leaderboards)
		{
			const uint64* Handle = Handles.Find(Def.Name);
			Report += FString::Printf(TEXT("  %s [%s]%s  handle %s\n"), *Def.Name.ToString(), *Def.Tag.ToString(),
				Def.bCreateIfMissing ? TEXT(" create-if-missing") : TEXT(""), Handle ? *FString::Printf(TEXT("%llu"), *Handle) : TEXT("(not looked up)"));
		}
	}

	for (const TPair<FName, uint64>& Pair : Handles)
	{
		if (!Definition || !Definition->FindLeaderboard(Pair.Key))
		{
			Report += FString::Printf(TEXT("  %s (by name)  handle %llu\n"), *Pair.Key.ToString(), Pair.Value);
		}
	}

	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
