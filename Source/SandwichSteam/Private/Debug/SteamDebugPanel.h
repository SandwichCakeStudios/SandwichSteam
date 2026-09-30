// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if SANDWICHSTEAM_WITH_DEBUG

class UWorld;

namespace SandwichSteam::Debug
{
	/** Steam.Debug.Show: shows or hides the debug panel as an overlay on the game viewport of World. */
	void ToggleOverlay(UWorld* World);

	/** Removes the overlay if it is showing (module shutdown). */
	void HideOverlay();

	/** Registers the sections of the core features (Core, User, Utility, Overlay). */
	void RegisterCoreSections();

	/** Unregisters the sections added by RegisterCoreSections. */
	void UnregisterCoreSections();
}

#endif // SANDWICHSTEAM_WITH_DEBUG
