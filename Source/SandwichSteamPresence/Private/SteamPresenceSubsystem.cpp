// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamPresenceSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "SteamPresenceBackend.h"
#include "SteamPresenceBatch.h"

using SandwichSteam::Presence::EIssue;

namespace
{
	FSteamResult MakeIssueResult(EIssue Issue, const FString& Key)
	{
		const FGameplayTag ErrorTag = Issue == EIssue::TooManyKeys ? FGameplayTag(SteamGameplayTags::Error_QuotaExceeded) : FGameplayTag(SteamGameplayTags::Error_InvalidArgument);
		return FSteamResult::Failure(ErrorTag, FText::Format(NSLOCTEXT("SandwichSteam", "PresenceRefused", "Rich presence key '{0}' was refused: {1}."),
			FText::FromString(Key), FText::FromString(SandwichSteam::Presence::DescribeIssue(Issue))));
	}
}

USteamPresenceSubsystem* USteamPresenceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamPresenceSubsystem>() : nullptr;
}

USteamPresenceSubsystem::USteamPresenceSubsystem() = default;
USteamPresenceSubsystem::~USteamPresenceSubsystem() = default;

FGameplayTag USteamPresenceSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Presence;
}

bool USteamPresenceSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	Definition = ToolSettings ? ToolSettings->LoadAppDefinition() : nullptr;
	if (!Definition)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam presence: no App Definition assigned in the Sandwich Steam settings. Only free keys can be set."));
	}

	if (!Batch.IsValid())
	{
		Batch = MakeShared<FSteamPresenceBatch>();
	}

	Backend = MakeShared<FSteamPresenceBackend>(this, Dispatcher.ToSharedRef());
	FlushCount = 0;
	SteamCalls = 0;

	// Keys from before a reactivation: start from a clean Steam side and send them all again.
	if (Batch->HasPending())
	{
		Backend->ClearAll();
		ScheduleFlush();
	}
	return true;
#else
	return false;
#endif
}

void USteamPresenceSubsystem::ShutdownFeature()
{
	// What was applied may be lost with the connection: send it again when the feature comes back.
	if (Batch.IsValid())
	{
		Batch->RequeueApplied();
	}

	FriendChanges.Reset();
	bFlushScheduled = false;
	Backend.Reset();
	Dispatcher.Reset();
	Definition = nullptr;
}

void USteamPresenceSubsystem::ScheduleFlush()
{
	if (bFlushScheduled || !Dispatcher.IsValid())
	{
		return;
	}

	bFlushScheduled = true;
	SANDWICHSTEAM_DISPATCH(Dispatcher, TWeakObjectPtr<USteamPresenceSubsystem>(this), [](USteamPresenceSubsystem& Presence)
	{
		Presence.Flush();
	});
}

void USteamPresenceSubsystem::Flush()
{
	bFlushScheduled = false;
	if (!Backend.IsValid() || !Batch.IsValid())
	{
		return; // Inactive: everything stays staged for the reactivation.
	}

	const FSteamPresenceBatch::FFlush Work = Batch->Flush();
	if (Work.IsEmpty())
	{
		return; // The staged changes cancelled each other out or matched what Steam already has.
	}

	int32 Calls = 0;
	const int32 Refused = Backend->Apply(Work, Calls);
	++FlushCount;
	SteamCalls += Calls;
	UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam presence: flushed %d set(s), %d removal(s)%s."), Work.Sets.Num(), Work.Removes.Num(), Work.bClearAll ? TEXT(", cleared first") : TEXT(""));
	if (Refused > 0)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam presence: Steam refused %d rich presence call(s)."), Refused);
	}
}

FSteamResult USteamPresenceSubsystem::SetPresenceByTag(const FGameplayTag& StatusTag, const TMap<FName, FString>& Args)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "PresenceNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so statuses cannot be set by tag."));
	}

	const FSteamPresenceDef* Def = Definition->FindPresence(StatusTag);
	if (!Def || Def->Token.IsEmpty())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "PresenceUnknownTag", "The tag '{0}' is not a rich presence status of the Steam App Definition."), FText::FromName(StatusTag.GetTagName())));
	}

	return SetPresenceByToken(Def->Token, Args);
}

FSteamResult USteamPresenceSubsystem::SetPresenceByToken(const FString& Token, const TMap<FName, FString>& Args)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Token.StartsWith(TEXT("#")) || Token.Len() < 2)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "PresenceBadToken", "'{0}' is not a rich presence token. Tokens start with # (for example #Status_InMatch) and must exist in the localization file uploaded to Steamworks. For plain text use Set Steam Presence Value with the key status."), FText::FromString(Token)));
	}

	// The row (optional) only tells which keys the status text uses.
	const FSteamPresenceDef* Def = Definition ? Definition->FindPresenceByToken(Token) : nullptr;

	// All or nothing: a refused key must not leave half a status staged.
	const FSteamPresenceBatch Backup = *Batch;

	EIssue Issue = Batch->Set(SandwichSteam::Presence::KeyDisplay, Token);
	FString BadKey = SandwichSteam::Presence::KeyDisplay;
	TSet<FString> NewKeys;

	for (const TPair<FName, FString>& Arg : Args)
	{
		if (Issue != EIssue::None)
		{
			break;
		}

		const FString Key = Arg.Key.IsNone() ? FString() : Arg.Key.ToString();
		if (SandwichSteam::Presence::IsReservedKey(Key))
		{
			*Batch = Backup;
			return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
				FText::Format(NSLOCTEXT("SandwichSteam", "PresenceReservedArg", "'{0}' is a key Steam reads itself and cannot be a status argument. Use Set Steam Presence Group or Set Steam Connect String."), FText::FromString(Key)));
		}

		if (Def && !Def->ExtraKeys.Contains(Arg.Key))
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam presence: '%s' is not listed as a key of the status '%s' in the App Definition. It is set anyway, but the text does not use it."), *Key, *Token);
		}

		BadKey = Key;
		Issue = Batch->Set(Key, Arg.Value);
		NewKeys.Add(Key);
	}

	if (Issue != EIssue::None)
	{
		*Batch = Backup;
		return MakeIssueResult(Issue, BadKey);
	}

	// The previous status may have set keys this one does not use.
	for (const FString& OldKey : LastStatusKeys)
	{
		if (!NewKeys.Contains(OldKey))
		{
			Batch->Remove(OldKey);
		}
	}

	LastStatusKeys = MoveTemp(NewKeys);
	ScheduleFlush();
	return FSteamResult::Success();
}

FSteamResult USteamPresenceSubsystem::SetPresenceValue(FName Key, const FString& Value)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const FString KeyString = Key.IsNone() ? FString() : Key.ToString();
	const EIssue Issue = Batch->Set(KeyString, Value);
	if (Issue != EIssue::None)
	{
		return MakeIssueResult(Issue, KeyString);
	}

	ScheduleFlush();
	return FSteamResult::Success();
}

FSteamResult USteamPresenceSubsystem::RemovePresenceValue(FName Key)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const FString KeyString = Key.IsNone() ? FString() : Key.ToString();
	const EIssue Issue = Batch->Remove(KeyString);
	if (Issue != EIssue::None)
	{
		return MakeIssueResult(Issue, KeyString);
	}

	ScheduleFlush();
	return FSteamResult::Success();
}

FSteamResult USteamPresenceSubsystem::SetGroup(const FString& GroupId, int32 Size)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (GroupId.IsEmpty())
	{
		Batch->Remove(SandwichSteam::Presence::KeyPlayerGroup);
		Batch->Remove(SandwichSteam::Presence::KeyPlayerGroupSize);
		ScheduleFlush();
		return FSteamResult::Success();
	}

	if (Size < 1)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "PresenceGroupSize", "A player group needs a size of at least 1."));
	}

	const FSteamPresenceBatch Backup = *Batch;
	EIssue Issue = Batch->Set(SandwichSteam::Presence::KeyPlayerGroup, GroupId);
	FString BadKey = SandwichSteam::Presence::KeyPlayerGroup;
	if (Issue == EIssue::None)
	{
		BadKey = SandwichSteam::Presence::KeyPlayerGroupSize;
		Issue = Batch->Set(SandwichSteam::Presence::KeyPlayerGroupSize, FString::FromInt(Size));
	}

	if (Issue != EIssue::None)
	{
		*Batch = Backup;
		return MakeIssueResult(Issue, BadKey);
	}

	ScheduleFlush();
	return FSteamResult::Success();
}

FSteamResult USteamPresenceSubsystem::SetConnectString(const FString& Connect)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const EIssue Issue = Batch->Set(SandwichSteam::Presence::KeyConnect, Connect);
	if (Issue != EIssue::None)
	{
		return MakeIssueResult(Issue, SandwichSteam::Presence::KeyConnect);
	}

	ScheduleFlush();
	return FSteamResult::Success();
}

FSteamResult USteamPresenceSubsystem::ClearPresence()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Batch->Clear();
	LastStatusKeys.Reset();
	ScheduleFlush();
	return FSteamResult::Success();
}

FString USteamPresenceSubsystem::GetFriendPresenceValue(FSteamId Friend, FName Key) const
{
	return (Backend.IsValid() && !Key.IsNone()) ? Backend->GetFriendValue(Friend, Key.ToString()) : FString();
}

TMap<FString, FString> USteamPresenceSubsystem::GetFriendPresence(FSteamId Friend) const
{
	TMap<FString, FString> Values;
	if (Backend.IsValid())
	{
		Backend->GetFriendValues(Friend, Values);
	}
	return Values;
}

void USteamPresenceSubsystem::RequestFriendPresence(FSteamId Friend)
{
	if (Backend.IsValid())
	{
		Backend->RequestFriend(Friend);
	}
}

void USteamPresenceSubsystem::HandleFriendPresenceUpdate(FSteamId Friend)
{
	if (!Backend.IsValid() || !Dispatcher.IsValid())
	{
		return;
	}

	if (FriendChanges.Add(Friend, 1))
	{
		// Queued from inside a drain, so it runs in the next one: every update of this frame joins the batch first.
		SANDWICHSTEAM_DISPATCH(Dispatcher, TWeakObjectPtr<USteamPresenceSubsystem>(this), [](USteamPresenceSubsystem& Presence)
		{
			Presence.FlushFriendChanges();
		});
	}
}

void USteamPresenceSubsystem::FlushFriendChanges()
{
	const TArray<TPair<FSteamId, uint32>> Changed = FriendChanges.Take();
	if (!Backend.IsValid())
	{
		return;
	}

	for (const TPair<FSteamId, uint32>& Entry : Changed)
	{
		OnFriendPresenceChanged.Broadcast(Entry.Key);
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamPresenceSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Presence: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	Report += FString::Printf(TEXT("  Definition: %s (%d statuses)\n"), Definition ? *Definition->GetName() : TEXT("none"), Definition ? Definition->Presence.Num() : 0);
	Report += FString::Printf(TEXT("  Flushes that sent something: %d, Steam calls: %d\n"), FlushCount, SteamCalls);

	if (Batch.IsValid())
	{
		Report += FString::Printf(TEXT("  Keys: %d wanted (limit %d), pending flush: %s\n"), Batch->GetDesiredCount(), SandwichSteam::Presence::MaxKeys, Batch->HasPending() ? TEXT("yes") : TEXT("no"));
		for (const TPair<FString, FString>& Pair : Batch->GetApplied())
		{
			Report += FString::Printf(TEXT("    %s = %s\n"), *Pair.Key, *Pair.Value);
		}
	}

	if (Definition)
	{
		for (const FSteamPresenceDef& Def : Definition->Presence)
		{
			FString Keys;
			for (const FName Key : Def.ExtraKeys)
			{
				Keys += (Keys.IsEmpty() ? TEXT("") : TEXT(", ")) + Key.ToString();
			}
			Report += FString::Printf(TEXT("  [%s] %s  keys: %s\n"), *Def.Tag.ToString(), *Def.Token, Keys.IsEmpty() ? TEXT("(none)") : *Keys);
		}
	}

	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
