// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamTest.h"
#include "SandwichSteamTestSubsystem.h"
#include "Engine/World.h"

namespace
{
	void ToggleTestPanel(UWorld* World)
	{
		if (USandwichSteamTestSubsystem* Subsystem = USandwichSteamTestSubsystem::Get(World))
		{
			Subsystem->ToggleTestPanel();
		}
	}
}

void FSandwichSteamTestModule::StartupModule()
{
	ToggleCommand = MakeUnique<FAutoConsoleCommandWithWorld>(
		TEXT("Steam.Test.Toggle"),
		TEXT("Shows or hides the Sandwich Steam test panel (Standalone or PIE)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ToggleTestPanel));
}

void FSandwichSteamTestModule::ShutdownModule()
{
	ToggleCommand.Reset();
}

IMPLEMENT_MODULE(FSandwichSteamTestModule, SandwichSteamTest)
