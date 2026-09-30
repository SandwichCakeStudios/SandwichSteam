// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamAchievements.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "SteamAchievementsSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpAchievements(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamAchievementsSubsystem* Achievements = FSteamDebugCommandSet::FindFeature<USteamAchievementsSubsystem>(World, Output))
		{
			Output.Log(*Achievements->BuildDebugString());
		}
	}

	void UnlockAchievement(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamAchievementsSubsystem* Achievements = FSteamDebugCommandSet::FindFeature<USteamAchievementsSubsystem>(World, Output);
		if (!Achievements)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Achievements.Unlock <ApiName>"));
			return;
		}

		const FSteamResult Result = Achievements->Unlock(FName(*Args[0]));
		Output.Logf(TEXT("Steam.Achievements.Unlock %s: %s"), *Args[0], Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
	}

	void ClearAchievement(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamAchievementsSubsystem* Achievements = FSteamDebugCommandSet::FindFeature<USteamAchievementsSubsystem>(World, Output);
		if (!Achievements)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Achievements.Clear <ApiName>"));
			return;
		}

		const FSteamResult Result = Achievements->Clear(FName(*Args[0]));
		Output.Logf(TEXT("Steam.Achievements.Clear %s: %s"), *Args[0], Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
	}

	const FName DebugSectionId(TEXT("Achievements"));

	FString ReportAchievements(UWorld* World)
	{
		const USteamAchievementsSubsystem* Achievements = SandwichSteam::Debug::FindFeatureSubsystem<USteamAchievementsSubsystem>(World);
		return Achievements ? Achievements->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}

	/** One Unlock and one Clear button per achievement. Empty until the stats arrived, then the panel rebuilds the buttons. */
	void BuildAchievementActions(UWorld* World, TArray<FSteamDebugAction>& OutActions)
	{
		const USteamAchievementsSubsystem* Achievements = SandwichSteam::Debug::FindFeatureSubsystem<USteamAchievementsSubsystem>(World);
		if (!Achievements)
		{
			return;
		}

		TArray<FSteamAchievementInfo> Infos;
		Achievements->GetAllInfo(Infos);
		for (const FSteamAchievementInfo& Info : Infos)
		{
			const FName ApiName = Info.ApiName;

			FSteamDebugAction Unlock;
			Unlock.Label = FText::Format(NSLOCTEXT("SandwichSteamAchievements", "DebugUnlock", "Unlock {0}"), FText::FromName(ApiName));
			Unlock.ToolTip = NSLOCTEXT("SandwichSteamAchievements", "DebugUnlockTip", "Unlocks the achievement and uploads it to Steam.");
			Unlock.Execute = [ApiName](UWorld* ActionWorld)
			{
				if (USteamAchievementsSubsystem* Subsystem = SandwichSteam::Debug::FindFeatureSubsystem<USteamAchievementsSubsystem>(ActionWorld))
				{
					const FSteamResult Result = Subsystem->Unlock(ApiName);
					UE_LOG(LogSandwichSteam, Log, TEXT("Debug: unlock %s: %s"), *ApiName.ToString(), Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
				}
			};
			OutActions.Add(MoveTemp(Unlock));

			FSteamDebugAction Clear;
			Clear.Label = FText::Format(NSLOCTEXT("SandwichSteamAchievements", "DebugClear", "Clear {0}"), FText::FromName(ApiName));
			Clear.ToolTip = NSLOCTEXT("SandwichSteamAchievements", "DebugClearTip", "Locks the achievement again (testing only).");
			Clear.Execute = [ApiName](UWorld* ActionWorld)
			{
				if (USteamAchievementsSubsystem* Subsystem = SandwichSteam::Debug::FindFeatureSubsystem<USteamAchievementsSubsystem>(ActionWorld))
				{
					const FSteamResult Result = Subsystem->Clear(ApiName);
					UE_LOG(LogSandwichSteam, Log, TEXT("Debug: clear %s: %s"), *ApiName.ToString(), Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
				}
			};
			OutActions.Add(MoveTemp(Clear));
		}
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamAchievementsModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamAchievements", "DebugTitle", "Achievements");
	Section.Order = 50;
	Section.BuildReport = &ReportAchievements;
	Section.BuildActions = &BuildAchievementActions;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Achievements.Dump"),
		TEXT("Prints every achievement with its unlock state and progress."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpAchievements));

	Commands.Add(TEXT("Steam.Achievements.Unlock"),
		TEXT("Unlocks an achievement: Steam.Achievements.Unlock <ApiName>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&UnlockAchievement));

	Commands.Add(TEXT("Steam.Achievements.Clear"),
		TEXT("Locks an achievement again (testing): Steam.Achievements.Clear <ApiName>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ClearAchievement));
#endif
}

void FSandwichSteamAchievementsModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamAchievementsModule, SandwichSteamAchievements)
