// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamStats.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "SteamStatsSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpStats(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamStatsSubsystem* Stats = FSteamDebugCommandSet::FindFeature<USteamStatsSubsystem>(World, Output))
		{
			Output.Log(*Stats->BuildDebugString());
		}
	}

	void ResetStats(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		if (USteamStatsSubsystem* Stats = FSteamDebugCommandSet::FindFeature<USteamStatsSubsystem>(World, Output))
		{
			const bool bAchievementsToo = Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0;
			const FSteamResult Result = Stats->DebugResetAll(bAchievementsToo);
			Output.Logf(TEXT("Steam.Stats.ResetAll (achievements: %s): %s"), bAchievementsToo ? TEXT("yes") : TEXT("no"),
				Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
		}
	}

	void ExportSchema(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamStatsSubsystem* Stats = FSteamDebugCommandSet::FindFeature<USteamStatsSubsystem>(World, Output))
		{
			FString FilePath;
			const FSteamResult Result = Stats->DebugExportSchema(FilePath);
			if (Result.IsSuccess())
			{
				Output.Logf(TEXT("Steam.Stats.ExportSchema wrote %s"), *FilePath);
			}
			else
			{
				Output.Logf(TEXT("Steam.Stats.ExportSchema failed: %s"), *Result.Message.ToString());
			}
		}
	}

	const FName DebugSectionId(TEXT("Stats"));

	FString ReportStats(UWorld* World)
	{
		const USteamStatsSubsystem* Stats = SandwichSteam::Debug::FindFeatureSubsystem<USteamStatsSubsystem>(World);
		return Stats ? Stats->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}

	void BuildStatsActions(UWorld* /*World*/, TArray<FSteamDebugAction>& OutActions)
	{
		FSteamDebugAction Store;
		Store.Label = NSLOCTEXT("SandwichSteamStats", "DebugStore", "Store stats now");
		Store.ToolTip = NSLOCTEXT("SandwichSteamStats", "DebugStoreTip", "Uploads changed stats immediately instead of waiting for the flush interval.");
		Store.Execute = [](UWorld* World)
		{
			if (USteamStatsSubsystem* Stats = SandwichSteam::Debug::FindFeatureSubsystem<USteamStatsSubsystem>(World))
			{
				const FSteamResult Result = Stats->StoreStatsNow();
				UE_LOG(LogSandwichSteam, Log, TEXT("Debug: store stats now: %s"), Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
			}
		};
		OutActions.Add(MoveTemp(Store));

		FSteamDebugAction Export;
		Export.Label = NSLOCTEXT("SandwichSteamStats", "DebugExport", "Export schema for the editor importer");
		Export.ToolTip = NSLOCTEXT("SandwichSteamStats", "DebugExportTip", "Writes Saved/SandwichSteam/Schema_<AppId>.json. Use Tools > Sandwich Steam > Import from Steam in the editor afterwards.");
		Export.Execute = [](UWorld* World)
		{
			if (const USteamStatsSubsystem* Stats = SandwichSteam::Debug::FindFeatureSubsystem<USteamStatsSubsystem>(World))
			{
				FString FilePath;
				const FSteamResult Result = Stats->DebugExportSchema(FilePath);
				if (Result.IsSuccess())
				{
					UE_LOG(LogSandwichSteam, Log, TEXT("Debug: schema written to %s"), *FilePath);
				}
				else
				{
					UE_LOG(LogSandwichSteam, Warning, TEXT("Debug: schema export failed: %s"), *Result.Message.ToString());
				}
			}
		};
		OutActions.Add(MoveTemp(Export));
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamStatsModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamStats", "DebugTitle", "Stats");
	Section.Order = 40;
	Section.BuildReport = &ReportStats;
	Section.BuildActions = &BuildStatsActions;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Stats.Dump"),
		TEXT("Prints whether the stats are ready, the upload state and the current value of every stat in the Steam App Definition."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpStats));

	Commands.Add(TEXT("Steam.Stats.ResetAll"),
		TEXT("Resets every stat of the local user: Steam.Stats.ResetAll [1 = also reset achievements]. Debug only, cannot be undone."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ResetStats));

	Commands.Add(TEXT("Steam.Stats.ExportSchema"),
		TEXT("Writes Saved/SandwichSteam/Schema_<AppId>.json with the achievements Steam reports and the stats of the App Definition."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ExportSchema));
#endif
}

void FSandwichSteamStatsModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamStatsModule, SandwichSteamStats)
