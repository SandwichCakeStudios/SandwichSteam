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
	 * With the Voice module installed, the Online Subsystem voice entries are added (Use Steam Voice Pipeline off) or removed (on).
	 * With bWithSteamAuth and the Auth module installed, the SteamAuth packet handler line is added (setting on) or removed (setting off).
	 * The validator passes false and checks that line in its own row.
	 */
	TArray<FSteamIniEntry> BuildEngineIniEntries(const USteamToolSettings& Settings, bool bWithSteamAuth = true);

	/** Everything Configure Steam wants in DefaultGame.ini for these settings (bRequiresPushToTalk for the Online Subsystem voice, plus the network tuning when it is on). */
	TArray<FSteamIniEntry> BuildGameIniEntries(const USteamToolSettings& Settings);

	/**
	 * "Configure Steam": writes the Online Subsystem Steam entries (DefaultPlatformService, bEnabled, SteamDevAppId, ...)
	 * into the project's DefaultEngine.ini from the App ID in Project Settings > Plugins > Sandwich Steam. With the Voice module installed
	 * it adds or removes the Online Subsystem voice entries (DefaultEngine.ini) to match Use Steam Voice Pipeline, and for the
	 * Online Subsystem voice writes bRequiresPushToTalk into DefaultGame.ini (never removed).
	 * Shows the pending changes first, checks the files out of source control and is idempotent.
	 */
	void ConfigureSteam();

	/** True when the Sessions module is installed, so the SteamSockets net driver entries belong in DefaultEngine.ini too. */
	bool WantsSessionsIni();

	/** True when the Voice module is installed, so Configure Steam manages the voice entries (see WantsOssVoiceIni). */
	bool WantsVoiceIni();

	/**
	 * True when the Voice module is installed and Use Steam Voice Pipeline is off, so the Online Subsystem voice entries belong in
	 * DefaultEngine.ini: [OnlineSubsystem] bHasVoiceEnabled and [Voice] bEnabled.
	 * False with the module installed means they must not be there (see Settings/SteamVoiceEditorAccess.h).
	 */
	bool WantsOssVoiceIni();

	/**
	 * True when the Auth module is installed and Enable Steam Authentication is on (Project Settings > Plugins > Sandwich Steam - Auth),
	 * so the SteamAuth packet handler line belongs in DefaultEngine.ini (see Settings/SteamAuthEditorAccess.h).
	 */
	bool WantsSteamAuthIni();
}
