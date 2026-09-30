// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

/** Developer-only module with an in-game Slate test panel for the plugin's features. Console: Steam.Test.Toggle. */
class FSandwichSteamTestModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	TUniquePtr<FAutoConsoleCommandWithWorld> ToggleCommand;
};
