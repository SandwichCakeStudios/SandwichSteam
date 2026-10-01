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
	 * On other platforms the command is copied to the clipboard. False (with a notification) when the SteamCMD path or account is missing.
	 * Prefer FSteamCmdSetupService::BeginTerminalLogin, which also checks the login once the editor is focused again.
	 */
	bool SandwichSteamCmdLoginTerminal();

	/** Runs "steamcmd +login <user> +quit" with the cached login (FSteamCmdSetupService) and reports the result. A Steam Guard prompt opens a dialog. */
	void TestSteamCmdLogin();
}
