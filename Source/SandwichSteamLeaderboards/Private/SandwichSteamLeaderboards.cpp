// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamLeaderboards.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "SteamLeaderboardsSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpLeaderboards(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamLeaderboardsSubsystem* Leaderboards = FSteamDebugCommandSet::FindFeature<USteamLeaderboardsSubsystem>(World, Output))
		{
			Output.Log(*Leaderboards->BuildDebugString());
		}
	}

	/** Steam.Leaderboards.Upload <Name> <Score>: keep-best upload. The answer is logged when Steam replies. */
	void UploadLeaderboardScore(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamLeaderboardsSubsystem* Leaderboards = FSteamDebugCommandSet::FindFeature<USteamLeaderboardsSubsystem>(World, Output);
		if (!Leaderboards)
		{
			return;
		}

		if (Args.Num() < 2)
		{
			Output.Log(TEXT("Usage: Steam.Leaderboards.Upload <Name> <Score>"));
			return;
		}

		const FName Name(*Args[0]);
		const FSteamResult Started = Leaderboards->UploadScore(Name, FCString::Atoi(*Args[1]), ESteamLeaderboardUploadMethod::KeepBest, TArray<int32>(),
			FSteamLeaderboardUploadDelegate::CreateLambda([Name](const FSteamResult& Result, const FSteamLeaderboardUploadResult& Upload)
			{
				if (Result.IsSuccess())
				{
					UE_LOG(LogSandwichSteam, Log, TEXT("Steam.Leaderboards.Upload %s: score %d, changed %s, rank %d -> %d"), *Name.ToString(), Upload.Score,
						Upload.bScoreChanged ? TEXT("yes") : TEXT("no"), Upload.PreviousRank, Upload.NewRank);
				}
				else
				{
					UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Leaderboards.Upload %s failed: %s"), *Name.ToString(), *Result.Message.ToString());
				}
			}));

		Output.Logf(TEXT("Steam.Leaderboards.Upload %s: %s"), *Args[0], Started.IsSuccess() ? TEXT("sent, the answer follows in the log") : *Started.Message.ToString());
	}

	/** Steam.Leaderboards.Top <Name> [Count]: logs the best entries when Steam replies. */
	void DownloadTopEntries(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamLeaderboardsSubsystem* Leaderboards = FSteamDebugCommandSet::FindFeature<USteamLeaderboardsSubsystem>(World, Output);
		if (!Leaderboards)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Leaderboards.Top <Name> [Count]"));
			return;
		}

		FSteamLeaderboardQuery Query;
		Query.Type = ESteamLeaderboardRequestType::Global;
		Query.RangeStart = 1;
		Query.RangeEnd = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 10;

		const FName Name(*Args[0]);
		const FSteamResult Started = Leaderboards->DownloadEntries(Name, Query,
			FSteamLeaderboardDownloadDelegate::CreateLambda([Name](const FSteamResult& Result, const TArray<FSteamLeaderboardEntry>& Entries)
			{
				if (!Result.IsSuccess())
				{
					UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Leaderboards.Top %s failed: %s"), *Name.ToString(), *Result.Message.ToString());
					return;
				}

				UE_LOG(LogSandwichSteam, Log, TEXT("Steam.Leaderboards.Top %s: %d entries"), *Name.ToString(), Entries.Num());
				for (const FSteamLeaderboardEntry& Entry : Entries)
				{
					UE_LOG(LogSandwichSteam, Log, TEXT("  #%d  %s  score %d  details %d"), Entry.Rank, *Entry.SteamId.ToString(), Entry.Score, Entry.Details.Num());
				}
			}));

		Output.Logf(TEXT("Steam.Leaderboards.Top %s: %s"), *Args[0], Started.IsSuccess() ? TEXT("sent, the answer follows in the log") : *Started.Message.ToString());
	}

	const FName DebugSectionId(TEXT("Leaderboards"));

	FString ReportLeaderboards(UWorld* World)
	{
		const USteamLeaderboardsSubsystem* Leaderboards = SandwichSteam::Debug::FindFeatureSubsystem<USteamLeaderboardsSubsystem>(World);
		return Leaderboards ? Leaderboards->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamLeaderboardsModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamLeaderboards", "DebugTitle", "Leaderboards");
	Section.Order = 60;
	Section.BuildReport = &ReportLeaderboards;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Leaderboards.Dump"),
		TEXT("Prints the leaderboards of the App Definition, cached handles and download counters."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpLeaderboards));

	Commands.Add(TEXT("Steam.Leaderboards.Upload"),
		TEXT("Uploads a score (keep best): Steam.Leaderboards.Upload <Name> <Score>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&UploadLeaderboardScore));

	Commands.Add(TEXT("Steam.Leaderboards.Top"),
		TEXT("Logs the best entries: Steam.Leaderboards.Top <Name> [Count]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DownloadTopEntries));
#endif
}

void FSandwichSteamLeaderboardsModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamLeaderboardsModule, SandwichSteamLeaderboards)
