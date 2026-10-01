// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

namespace SandwichSteam::Editor
{
	/**
	 * Registers the dashboard's "Setup" page (first in the nav). Lists the five things publishing needs (App ID, App Definition,
	 * SteamCMD, Steam account, depots) with one button each that does the work, and runs what it can on its own (login check,
	 * depot fetch). Only shown while something is missing; once shown it stays until the tab closes, ending on "You're all set".
	 * Call from StartupModule.
	 */
	void RegisterSetupDashboardPage();
}
