// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamFriendsSubsystem.h"
#include "Containers/Ticker.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/Overlay/SteamOverlaySubsystem.h"
#include "SteamFriendsBackend.h"

namespace
{
	/** How long a read waits for Steam to deliver the names of users who are not friends. Then it completes with what it has. */
	constexpr float PendingReadTimeoutSeconds = 5.0f;

	bool PassesFilter(const FSteamFriendInfo& Info, ESteamFriendFilter Filter)
	{
		switch (Filter)
		{
		case ESteamFriendFilter::Online:
			return Info.PersonaState != ESteamPersonaState::Offline;
		case ESteamFriendFilter::InGame:
			return Info.bInGame;
		case ESteamFriendFilter::InThisGame:
			return Info.bInThisGame;
		default:
			return true;
		}
	}
}

/** A read that waits for Steam to deliver user data. Hidden from the public header. */
struct USteamFriendsSubsystem::FPendingRead
{
	uint32 Id = 0;
	ESteamFriendSource Source = ESteamFriendSource::Friends;
	ESteamFriendFilter Filter = ESteamFriendFilter::All;
	FSteamReadFriendsDelegate OnComplete;
	TArray<FSteamId> Ids;
	TSet<FSteamId> Waiting;
	FTSTicker::FDelegateHandle Timeout;
};

USteamFriendsSubsystem* USteamFriendsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamFriendsSubsystem>() : nullptr;
}

USteamFriendsSubsystem::USteamFriendsSubsystem() = default;
USteamFriendsSubsystem::~USteamFriendsSubsystem() = default;

FGameplayTag USteamFriendsSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Friends;
}

bool USteamFriendsSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	Backend = MakeShared<FSteamFriendsBackend>(this, Dispatcher.ToSharedRef());
	PersonaChanges = 0;
	Broadcasts = 0;
	return true;
#else
	return false;
#endif
}

void USteamFriendsSubsystem::ShutdownFeature()
{
	// Reads that are still waiting are dropped without a call (the async node reports Cancelled itself).
	for (const TSharedRef<FPendingRead>& Read : PendingReads)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Read->Timeout);
	}
	PendingReads.Reset();

	Changes.Reset();
	Backend.Reset();
	Dispatcher.Reset();
}

FSteamResult USteamFriendsSubsystem::ReadFriends(ESteamFriendSource Source, ESteamFriendFilter Filter, FSteamReadFriendsDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const TSharedRef<FPendingRead> Read = MakeShared<FPendingRead>();
	Read->Id = NextReadId++;
	Read->Source = Source;
	Read->Filter = Filter;
	Read->OnComplete = MoveTemp(OnComplete);
	Backend->CollectIds(Source, Read->Ids);

	// Ask Steam for every user whose data is missing (only ever the case for users who are not friends).
	FSteamFriendInfo Info;
	for (const FSteamId& Id : Read->Ids)
	{
		if (!Backend->ReadInfo(Id, /*bRequestIfMissing*/ true, Info))
		{
			Read->Waiting.Add(Id);
		}
	}

	if (Read->Waiting.IsEmpty())
	{
		CompleteRead(Read);
		return FSteamResult::Success();
	}

	UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam friends: read %d is waiting for the data of %d user(s)."), Read->Id, Read->Waiting.Num());
	Read->Timeout = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, ReadId = Read->Id](float)
	{
		FinishRead(ReadId, /*bTimedOut*/ true);
		return false;
	}), PendingReadTimeoutSeconds);
	PendingReads.Add(Read);
	return FSteamResult::Success();
}

void USteamFriendsSubsystem::FinishRead(uint32 ReadId, bool bTimedOut)
{
	const int32 Index = PendingReads.IndexOfByPredicate([ReadId](const TSharedRef<FPendingRead>& Read) { return Read->Id == ReadId; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	const TSharedRef<FPendingRead> Read = PendingReads[Index];
	PendingReads.RemoveAt(Index);
	if (!bTimedOut)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Read->Timeout);
	}
	else
	{
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam friends: read %d timed out waiting for %d user(s); completing with what Steam has."), Read->Id, Read->Waiting.Num());
	}

	CompleteRead(Read);
}

void USteamFriendsSubsystem::CompleteRead(const TSharedRef<FPendingRead>& Read)
{
	TArray<FSteamFriendInfo> List;
	if (Backend.IsValid())
	{
		List.Reserve(Read->Ids.Num());
		FSteamFriendInfo Info;
		for (const FSteamId& Id : Read->Ids)
		{
			Backend->ReadInfo(Id, /*bRequestIfMissing*/ false, Info);
			if (PassesFilter(Info, Read->Filter))
			{
				List.Add(Info);
			}
		}

		// Online users first, then by name. Case insensitive so "alice" and "Bob" sort as people expect.
		List.Sort([](const FSteamFriendInfo& A, const FSteamFriendInfo& B)
		{
			const bool bOnlineA = A.PersonaState != ESteamPersonaState::Offline;
			const bool bOnlineB = B.PersonaState != ESteamPersonaState::Offline;
			if (bOnlineA != bOnlineB)
			{
				return bOnlineA;
			}
			return A.Name.Compare(B.Name, ESearchCase::IgnoreCase) < 0;
		});
	}

	Read->OnComplete.ExecuteIfBound(FSteamResult::Success(), List);
}

int32 USteamFriendsSubsystem::GetFriendCount() const
{
	return Backend.IsValid() ? Backend->GetFriendCount() : 0;
}

bool USteamFriendsSubsystem::IsFriend(FSteamId Id) const
{
	return Backend.IsValid() && Backend->IsFriend(Id);
}

bool USteamFriendsSubsystem::GetFriendInfo(FSteamId Id, FSteamFriendInfo& OutInfo) const
{
	if (!Backend.IsValid() || !Id.IsValid())
	{
		OutInfo = FSteamFriendInfo();
		return false;
	}

	Backend->ReadInfo(Id, /*bRequestIfMissing*/ true, OutInfo);
	return true;
}

TArray<FSteamFriendGroup> USteamFriendsSubsystem::GetFriendGroups() const
{
	TArray<FSteamFriendGroup> Groups;
	if (Backend.IsValid())
	{
		Backend->ReadGroups(Groups);
	}
	return Groups;
}

FSteamResult USteamFriendsSubsystem::RequireOverlay(USteamOverlaySubsystem*& OutOverlay) const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	OutOverlay = GameInstance ? GameInstance->GetSubsystem<USteamOverlaySubsystem>() : nullptr;
	if (!OutOverlay)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "FriendsNoOverlay", "The Overlay feature is disabled, so the overlay page cannot be opened."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamFriendsSubsystem::OpenFriendsList()
{
	USteamOverlaySubsystem* Overlay = nullptr;
	const FSteamResult Result = RequireOverlay(Overlay);
	return Result.IsSuccess() ? Overlay->OpenDialog(ESteamOverlayDialog::Friends) : Result;
}

FSteamResult USteamFriendsSubsystem::OpenProfile(FSteamId Id)
{
	USteamOverlaySubsystem* Overlay = nullptr;
	const FSteamResult Result = RequireOverlay(Overlay);
	return Result.IsSuccess() ? Overlay->OpenTargetDialog(ESteamOverlayDialog::UserProfile, Id) : Result;
}

FSteamResult USteamFriendsSubsystem::OpenChat(FSteamId Id)
{
	USteamOverlaySubsystem* Overlay = nullptr;
	const FSteamResult Result = RequireOverlay(Overlay);
	return Result.IsSuccess() ? Overlay->OpenTargetDialog(ESteamOverlayDialog::UserChat, Id) : Result;
}

FSteamResult USteamFriendsSubsystem::OpenAddFriend(FSteamId Id)
{
	USteamOverlaySubsystem* Overlay = nullptr;
	const FSteamResult Result = RequireOverlay(Overlay);
	return Result.IsSuccess() ? Overlay->OpenTargetDialog(ESteamOverlayDialog::AddFriend, Id) : Result;
}

void USteamFriendsSubsystem::HandlePersonaChange(FSteamId UserId, uint32 Flags)
{
	if (!Backend.IsValid() || !Dispatcher.IsValid())
	{
		return;
	}

	++PersonaChanges;

	// Data that a read waits for has arrived (or at least changed): check the waiting reads.
	if (!PendingReads.IsEmpty())
	{
		TArray<uint32> Done;
		for (const TSharedRef<FPendingRead>& Read : PendingReads)
		{
			if (Read->Waiting.Remove(UserId) > 0 && Read->Waiting.IsEmpty())
			{
				Done.Add(Read->Id);
			}
		}

		for (const uint32 ReadId : Done)
		{
			FinishRead(ReadId, /*bTimedOut*/ false);
		}
	}

	// Events are for friends (and for users who just stopped being one).
	if (!Backend.IsValid() || (!Backend->IsFriend(UserId) && (Flags & static_cast<uint32>(ESteamFriendChange::Relationship)) == 0))
	{
		return;
	}

	if (Changes.Add(UserId, Flags))
	{
		// Queued from inside a drain, so it runs in the next one: every change of this frame joins the batch first.
		SANDWICHSTEAM_DISPATCH(Dispatcher, TWeakObjectPtr<USteamFriendsSubsystem>(this), [](USteamFriendsSubsystem& Friends)
		{
			Friends.FlushChanges();
		});
	}
}

void USteamFriendsSubsystem::FlushChanges()
{
	const TArray<TPair<FSteamId, uint32>> Batch = Changes.Take();
	if (!Backend.IsValid())
	{
		return;
	}

	bool bListChanged = false;
	for (const TPair<FSteamId, uint32>& Change : Batch)
	{
		FSteamFriendInfo Info;
		Backend->ReadInfo(Change.Key, /*bRequestIfMissing*/ false, Info);

		if ((Change.Value & static_cast<uint32>(ESteamFriendChange::Relationship)) != 0)
		{
			bListChanged = true;
		}

		++Broadcasts;
		OnFriendStateChanged.Broadcast(Info, static_cast<int32>(Change.Value));
	}

	if (bListChanged)
	{
		OnFriendsListChanged.Broadcast();
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamFriendsSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Friends: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!Backend.IsValid())
	{
		return Report.TrimEnd();
	}

	TArray<FSteamId> Ids;
	Backend->CollectIds(ESteamFriendSource::Friends, Ids);

	int32 Online = 0;
	int32 InGame = 0;
	int32 InThisGame = 0;
	FSteamFriendInfo Info;
	for (const FSteamId& Id : Ids)
	{
		Backend->ReadInfo(Id, false, Info);
		Online += Info.PersonaState != ESteamPersonaState::Offline ? 1 : 0;
		InGame += Info.bInGame ? 1 : 0;
		InThisGame += Info.bInThisGame ? 1 : 0;
	}

	Report += FString::Printf(TEXT("  Friends: %d (online %d, in a game %d, in this game %d)\n"), Ids.Num(), Online, InGame, InThisGame);
	Report += FString::Printf(TEXT("  Friend groups: %d\n"), GetFriendGroups().Num());
	Report += FString::Printf(TEXT("  Persona changes received: %d, events broadcast: %d (coalesced), waiting reads: %d, changes pending: %d\n"),
		PersonaChanges, Broadcasts, PendingReads.Num(), Changes.Num());

	const int32 Shown = FMath::Min(Ids.Num(), 15);
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		Backend->ReadInfo(Ids[Index], false, Info);
		Report += FString::Printf(TEXT("  %s  %s  state %d%s%s\n"), *Info.SteamId.ToString(), *Info.Name, static_cast<int32>(Info.PersonaState),
			Info.bInGame ? *FString::Printf(TEXT("  game %d"), Info.GameAppId) : TEXT(""), Info.bInThisGame ? TEXT(" (this game)") : TEXT(""));
	}
	if (Ids.Num() > Shown)
	{
		Report += FString::Printf(TEXT("  ... and %d more\n"), Ids.Num() - Shown);
	}

	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
