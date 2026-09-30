// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Makes sure the Steam App Definition assigned in the Sandwich Steam settings is part of every cook.
 *
 * A soft reference held only by a developer settings object is not followed by the cooker, so the definition would be
 * missing from a packaged game. The helper adds the asset to "Primary Asset Types to Scan" (type SteamAppDefinition,
 * cook rule Always Cook) in DefaultGame.ini. It runs once at editor startup and whenever the assigned asset changes.
 */
class FSteamCookHelper
{
public:
	void Register();
	void Unregister();

	/** Adds the assigned definition to the Asset Manager settings. Returns true when the project config was changed. */
	static bool EnsureAppDefinitionCooked();

	/** True when the assigned definition is found by a Primary Asset Type to Scan with a cook rule other than Never Cook. False when none is assigned. */
	static bool IsAppDefinitionCooked();

private:
	void HandleAppDefinitionChanged();

	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle SettingsChangedHandle;
};
