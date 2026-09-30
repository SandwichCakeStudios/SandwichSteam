// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

#if SANDWICHSTEAM_WITH_DEBUG

/**
 * Console command registry for the Steam.* debug commands (non-Shipping builds only).
 * Features add their own `Steam.<Feature>.Dump` through RegisterCommand() from their module-level setup.
 */
namespace SandwichSteam::Debug
{
	/** Registers a command that receives the args, the world of the console context (may be null) and an output device. */
	void RegisterCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Delegate);

	/** Registers Steam.Core.Dump and Steam.Core.ThreadCheck. Called from StartupModule. */
	void RegisterCoreCommands();

	/** Registers Steam.User.Dump, Steam.Utility.Dump, Steam.Overlay.Dump and Steam.Overlay.Open (SteamFeatureCommands.cpp). Called from StartupModule. */
	void RegisterFeatureCommands();

	/** Unregisters every command. Called from ShutdownModule. */
	void UnregisterCommands();
}

#endif // SANDWICHSTEAM_WITH_DEBUG
