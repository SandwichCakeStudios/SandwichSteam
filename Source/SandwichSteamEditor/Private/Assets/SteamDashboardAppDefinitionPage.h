// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

namespace SandwichSteam::Editor
{
	/**
	 * Registers the dashboard's "App Definition" page: a summary of the assigned Steam App Definition (row counts,
	 * or a Create-and-assign button when none is assigned), an Open asset button, and the three actions that
	 * otherwise only exist as buttons buried inside the asset's own Details panel (Import from Steam, Generate
	 * Steam Input Actions File, Generate Rich Presence Localization). Call from StartupModule.
	 */
	void RegisterAppDefinitionDashboardPage();
}
