// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamDLC.h"
#include "Debug/SteamDebugSection.h"
#include "SteamDLCSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpDLC(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamDLCSubsystem* DLC = FSteamDebugCommandSet::FindFeature<USteamDLCSubsystem>(World, Output))
		{
			Output.Log(*DLC->BuildDebugString());
		}
	}

	const FName DebugSectionId(TEXT("DLC"));

	FString ReportDLC(UWorld* World)
	{
		const USteamDLCSubsystem* DLC = SandwichSteam::Debug::FindFeatureSubsystem<USteamDLCSubsystem>(World);
		return DLC ? DLC->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamDLCModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamDLC", "DebugTitle", "DLC");
	Section.Order = 90;
	Section.BuildReport = &ReportDLC;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.DLC.Dump"),
		TEXT("Prints the DLC rows of the App Definition and every DLC Steam lists, with owned and installed state."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpDLC));
#endif
}

void FSandwichSteamDLCModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamDLCModule, SandwichSteamDLC)
