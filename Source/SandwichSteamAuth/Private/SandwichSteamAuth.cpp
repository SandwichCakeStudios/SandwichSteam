// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamAuth.h"
#include "Debug/SteamDebugSection.h"
#include "SteamAuthSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void DumpAuth(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamAuthSubsystem* Auth = FSteamDebugCommandSet::FindFeature<USteamAuthSubsystem>(World, Output))
		{
			Output.Log(*Auth->BuildDebugString());
		}
	}

	const FName DebugSectionId(TEXT("Auth"));

	FString ReportAuth(UWorld* World)
	{
		const USteamAuthSubsystem* Auth = SandwichSteam::Debug::FindFeatureSubsystem<USteamAuthSubsystem>(World);
		return Auth ? Auth->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamAuthModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamAuth", "DebugTitle", "Auth");
	Section.Order = 95;
	Section.BuildReport = &ReportAuth;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Auth.Dump"),
		TEXT("Prints the Auth setting, whether the SteamAuth packet handler is loaded, the failure policy, the delegate bindings and every player's auth status on this host."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpAuth));
#endif
}

void FSandwichSteamAuthModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamAuthModule, SandwichSteamAuth)
