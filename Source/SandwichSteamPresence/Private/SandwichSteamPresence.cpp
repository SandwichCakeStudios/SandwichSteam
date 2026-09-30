// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamPresence.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "GameplayTagsManager.h"
#include "SteamPresenceSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpPresence(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamPresenceSubsystem* Presence = FSteamDebugCommandSet::FindFeature<USteamPresenceSubsystem>(World, Output))
		{
			Output.Log(*Presence->BuildDebugString());
		}
	}

	void LogResult(FOutputDevice& Output, const TCHAR* Command, const FSteamResult& Result)
	{
		Output.Logf(TEXT("%s: %s"), Command, Result.IsSuccess() ? TEXT("staged, sent with the next frame") : *Result.Message.ToString());
	}

	/** Steam.Presence.Set <Tag> [Key=Value ...]: shows a status of the App Definition, for example Steam.Presence.Set Steam.Presence.InMatch map=Harbor. */
	void SetPresenceStatus(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamPresenceSubsystem* Presence = FSteamDebugCommandSet::FindFeature<USteamPresenceSubsystem>(World, Output);
		if (!Presence)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Presence.Set <Tag> [Key=Value ...]"));
			return;
		}

		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			Output.Logf(TEXT("The gameplay tag '%s' does not exist."), *Args[0]);
			return;
		}

		TMap<FName, FString> Values;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			FString Key;
			FString Value;
			if (Args[Index].Split(TEXT("="), &Key, &Value))
			{
				Values.Add(FName(*Key), Value);
			}
		}

		LogResult(Output, TEXT("Steam.Presence.Set"), Presence->SetPresenceByTag(Tag, Values));
	}

	/** Steam.Presence.Raw <Key> [Value]: sets a free key, for example Steam.Presence.Raw status Testing. An empty value removes the key. */
	void SetPresenceRaw(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamPresenceSubsystem* Presence = FSteamDebugCommandSet::FindFeature<USteamPresenceSubsystem>(World, Output);
		if (!Presence)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Presence.Raw <Key> [Value]"));
			return;
		}

		FString Value;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			Value += (Index > 1 ? TEXT(" ") : TEXT("")) + Args[Index];
		}

		LogResult(Output, TEXT("Steam.Presence.Raw"), Presence->SetPresenceValue(FName(*Args[0]), Value));
	}

	void ClearPresence(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (USteamPresenceSubsystem* Presence = FSteamDebugCommandSet::FindFeature<USteamPresenceSubsystem>(World, Output))
		{
			LogResult(Output, TEXT("Steam.Presence.Clear"), Presence->ClearPresence());
		}
	}

	/** Steam.Presence.Friend <SteamId>: prints the rich presence keys of a friend. */
	void PrintFriendPresence(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		const USteamPresenceSubsystem* Presence = FSteamDebugCommandSet::FindFeature<USteamPresenceSubsystem>(World, Output);
		if (!Presence)
		{
			return;
		}

		FSteamId Friend;
		if (Args.IsEmpty() || !FSteamId::FromString(Args[0], Friend))
		{
			Output.Log(TEXT("Usage: Steam.Presence.Friend <SteamID64>"));
			return;
		}

		const TMap<FString, FString> Values = Presence->GetFriendPresence(Friend);
		Output.Logf(TEXT("Steam.Presence.Friend %s: %d key(s)"), *Friend.ToString(), Values.Num());
		for (const TPair<FString, FString>& Pair : Values)
		{
			Output.Logf(TEXT("  %s = %s"), *Pair.Key, *Pair.Value);
		}
	}

	const FName DebugSectionId(TEXT("Presence"));

	FString ReportPresence(UWorld* World)
	{
		const USteamPresenceSubsystem* Presence = SandwichSteam::Debug::FindFeatureSubsystem<USteamPresenceSubsystem>(World);
		return Presence ? Presence->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamPresenceModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamPresence", "DebugTitle", "Rich Presence");
	Section.Order = 80;
	Section.BuildReport = &ReportPresence;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Presence.Dump"),
		TEXT("Prints the statuses of the App Definition, the keys set and the flush counters."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpPresence));

	Commands.Add(TEXT("Steam.Presence.Set"),
		TEXT("Shows a status of the App Definition: Steam.Presence.Set <Tag> [Key=Value ...]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetPresenceStatus));

	Commands.Add(TEXT("Steam.Presence.Raw"),
		TEXT("Sets a free rich presence key: Steam.Presence.Raw <Key> [Value]. Try 'status' on any App ID."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetPresenceRaw));

	Commands.Add(TEXT("Steam.Presence.Clear"),
		TEXT("Removes every rich presence key."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ClearPresence));

	Commands.Add(TEXT("Steam.Presence.Friend"),
		TEXT("Prints the rich presence of a friend: Steam.Presence.Friend <SteamID64>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PrintFriendPresence));
#endif
}

void FSandwichSteamPresenceModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamPresenceModule, SandwichSteamPresence)
