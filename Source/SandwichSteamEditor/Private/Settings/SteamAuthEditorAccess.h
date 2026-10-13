// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Editor access to the optional Auth module's settings (Project Settings > Plugins > Sandwich Steam - Auth).
 * The editor does not depend on feature modules, so the settings object is found by class path and read / written through
 * reflection. Everything returns false / does nothing when the Auth module is not installed.
 */
namespace SandwichSteam::Editor::Auth
{
	/** True when the SandwichSteamAuth module is loaded. */
	bool IsModuleInstalled();

	/** Enable Steam Authentication, as Project Settings shows it right now. */
	bool IsEnabled();

	/** Sets Enable Steam Authentication and saves it to DefaultGame.ini. */
	void SetEnabled(bool bEnabled);

	/** True when the Failure Policy is Custom (nobody is kicked automatically). */
	bool IsCustomPolicy();

	/** Sets the Failure Policy (Custom or Kick) and saves it to DefaultGame.ini. */
	void SetCustomPolicy(bool bCustom);

	/** True when the project's DefaultEngine.ini has the SteamAuth packet handler line. Reads the file: do not call every frame. */
	bool HasIniLine();
}
