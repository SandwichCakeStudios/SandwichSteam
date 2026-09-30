// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SandwichSteam::Editor
{
	/** Modal Steam Guard code box. Unset when cancelled. Call it from a ticker, not from inside a delegate broadcast. */
	TOptional<FString> PromptSteamGuardCode(bool bMobile);

	/** Writes app_build / depot_build scripts for the first configured branch into Saved/SandwichSteam/Publish (nothing is uploaded). */
	void GenerateVdfDryRun();

	/**
	 * Opens a terminal running "steamcmd +login <user>" for the one-time password and Steam Guard step (Windows).
	 * On other platforms the command is copied to the clipboard.
	 */
	void SandwichSteamCmdLoginTerminal();

	/** Runs "steamcmd +login <user> +quit" with the cached login and reports the result. A Steam Guard prompt opens a dialog. */
	void TestSteamCmdLogin();
}
