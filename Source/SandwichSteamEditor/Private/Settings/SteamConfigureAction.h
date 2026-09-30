// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SandwichSteam::Editor
{
	/**
	 * "Configure Steam": writes the Online Subsystem Steam entries (DefaultPlatformService, bEnabled, SteamDevAppId, ...)
	 * into the project's DefaultEngine.ini from the App ID in Project Settings > Plugins > Sandwich Steam, and the voice
	 * entry into DefaultGame.ini when the Voice module is installed.
	 * Shows the pending changes first, checks the files out of source control and is idempotent.
	 */
	void ConfigureSteam();

	/** True when the Sessions module is installed, so the SteamSockets net driver entries belong in DefaultEngine.ini too. */
	bool WantsSessionsIni();

	/** True when the Voice module is installed, so [OnlineSubsystem] bHasVoiceEnabled (DefaultEngine.ini) and bRequiresPushToTalk (DefaultGame.ini) are needed. */
	bool WantsVoiceIni();
}
