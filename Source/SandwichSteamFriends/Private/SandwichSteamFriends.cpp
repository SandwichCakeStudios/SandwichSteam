// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamFriends.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "SteamFriendsSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpFriends(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamFriendsSubsystem* Friends = FSteamDebugCommandSet::FindFeature<USteamFriendsSubsystem>(World, Output))
		{
			Output.Log(*Friends->BuildDebugString());
		}
	}

	/** Steam.Friends.List [All|Online|InGame|InThisGame] [Friends|Blocked|Recent]: logs the list when it is read. */
	void ListFriends(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamFriendsSubsystem* Friends = FSteamDebugCommandSet::FindFeature<USteamFriendsSubsystem>(World, Output);
		if (!Friends)
		{
			return;
		}

		ESteamFriendFilter Filter = ESteamFriendFilter::All;
		if (Args.Num() > 0)
		{
			if (Args[0].Equals(TEXT("Online"), ESearchCase::IgnoreCase)) { Filter = ESteamFriendFilter::Online; }
			else if (Args[0].Equals(TEXT("InGame"), ESearchCase::IgnoreCase)) { Filter = ESteamFriendFilter::InGame; }
			else if (Args[0].Equals(TEXT("InThisGame"), ESearchCase::IgnoreCase)) { Filter = ESteamFriendFilter::InThisGame; }
		}

		ESteamFriendSource Source = ESteamFriendSource::Friends;
		if (Args.Num() > 1)
		{
			if (Args[1].Equals(TEXT("Blocked"), ESearchCase::IgnoreCase)) { Source = ESteamFriendSource::Blocked; }
			else if (Args[1].StartsWith(TEXT("Recent"), ESearchCase::IgnoreCase)) { Source = ESteamFriendSource::RecentPlayers; }
		}

		const FSteamResult Started = Friends->ReadFriends(Source, Filter,
			FSteamReadFriendsDelegate::CreateLambda([](const FSteamResult& Result, const TArray<FSteamFriendInfo>& List)
			{
				if (!Result.IsSuccess())
				{
					UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Friends.List failed: %s"), *Result.Message.ToString());
					return;
				}

				UE_LOG(LogSandwichSteam, Log, TEXT("Steam.Friends.List: %d user(s)"), List.Num());
				for (const FSteamFriendInfo& Info : List)
				{
					UE_LOG(LogSandwichSteam, Log, TEXT("  %s  %s  state %d  game %d%s%s"), *Info.SteamId.ToString(), *Info.Name, static_cast<int32>(Info.PersonaState), Info.GameAppId,
						Info.bInThisGame ? TEXT(" (this game)") : TEXT(""), Info.bInfoLoaded ? TEXT("") : TEXT(" (data pending)"));
				}
			}));

		Output.Logf(TEXT("Steam.Friends.List: %s"), Started.IsSuccess() ? TEXT("the list follows in the log") : *Started.Message.ToString());
	}

	const FName DebugSectionId(TEXT("Friends"));

	FString ReportFriends(UWorld* World)
	{
		const USteamFriendsSubsystem* Friends = SandwichSteam::Debug::FindFeatureSubsystem<USteamFriendsSubsystem>(World);
		return Friends ? Friends->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamFriendsModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamFriends", "DebugTitle", "Friends");
	Section.Order = 70;
	Section.BuildReport = &ReportFriends;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Friends.Dump"),
		TEXT("Prints the friend counts, coalescing counters and the first friends."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpFriends));

	Commands.Add(TEXT("Steam.Friends.List"),
		TEXT("Logs a list of users: Steam.Friends.List [All|Online|InGame|InThisGame] [Friends|Blocked|Recent]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ListFriends));
#endif
}

void FSandwichSteamFriendsModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamFriendsModule, SandwichSteamFriends)
