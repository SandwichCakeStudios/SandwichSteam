// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamScreenshots.h"
#include "Debug/SteamDebugSection.h"
#include "SteamScreenshotsSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpScreenshots(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamScreenshotsSubsystem* Screenshots = FSteamDebugCommandSet::FindFeature<USteamScreenshotsSubsystem>(World, Output))
		{
			Output.Log(*Screenshots->BuildDebugString());
		}
	}

	/** Steam.Screenshot [trigger|capture]: trigger asks Steam to capture, capture hands the game viewport to Steam (default). */
	void TakeScreenshot(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamScreenshotsSubsystem* Screenshots = FSteamDebugCommandSet::FindFeature<USteamScreenshotsSubsystem>(World, Output);
		if (!Screenshots)
		{
			return;
		}

		const bool bTrigger = !Args.IsEmpty() && Args[0].Equals(TEXT("trigger"), ESearchCase::IgnoreCase);
		const FSteamResult Result = bTrigger ? Screenshots->TriggerScreenshot() : Screenshots->CaptureViewportToSteam();
		Output.Logf(TEXT("Steam.Screenshot %s: %s"), bTrigger ? TEXT("trigger") : TEXT("capture"), Result.IsSuccess() ? TEXT("requested, check the Steam screenshot library") : *Result.Message.ToString());
	}

	const FName DebugSectionId(TEXT("Screenshots"));

	FString ReportScreenshots(UWorld* World)
	{
		const USteamScreenshotsSubsystem* Screenshots = SandwichSteam::Debug::FindFeatureSubsystem<USteamScreenshotsSubsystem>(World);
		return Screenshots ? Screenshots->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamScreenshotsModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamScreenshots", "DebugTitle", "Screenshots");
	Section.Order = 110;
	Section.BuildReport = &ReportScreenshots;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Screenshot.Dump"),
		TEXT("Prints hook mode, location, tagged users and the screenshot counters."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpScreenshots));

	Commands.Add(TEXT("Steam.Screenshot"),
		TEXT("Takes a screenshot: Steam.Screenshot [trigger|capture]. capture (default) hands the game viewport to Steam, trigger asks Steam to capture."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&TakeScreenshot));
#endif
}

void FSandwichSteamScreenshotsModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamScreenshotsModule, SandwichSteamScreenshots)
