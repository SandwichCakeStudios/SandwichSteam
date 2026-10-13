// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Settings/SteamIniWriter.h"

class USteamToolSettings;

namespace SandwichSteam::Editor
{
	/** The network tuning values as set in Project Settings (Networking category). */
	FSteamNetworkTuning MakeNetworkTuning(const USteamToolSettings& Settings);

	/**
	 * Everything Configure Steam wants in DefaultEngine.ini for these settings (required entries, plus the network tuning when it is on).
	 * With bWithSteamAuth and the Auth module installed, the SteamAuth packet handler line is added (setting on) or removed (setting off).
	 * The validator passes false and checks that line in its own row.
	 */
	TArray<FSteamIniEntry> BuildEngineIniEntries(const USteamToolSettings& Settings, bool bWithSteamAuth = true);

	/** Everything Configure Steam wants in DefaultGame.ini for these settings (voice, plus the network tuning when it is on). */
	TArray<FSteamIniEntry> BuildGameIniEntries(const USteamToolSettings& Settings);

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

	/**
	 * True when the Auth module is installed and Enable Steam Authentication is on (Project Settings > Plugins > Sandwich Steam - Auth),
	 * so the SteamAuth packet handler line belongs in DefaultEngine.ini (see Settings/SteamAuthEditorAccess.h).
	 */
	bool WantsSteamAuthIni();
}
