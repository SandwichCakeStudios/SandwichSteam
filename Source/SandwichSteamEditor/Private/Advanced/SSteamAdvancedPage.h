// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

namespace SandwichSteam::Editor
{
	/**
	 * Registers the dashboard's "Advanced" page, pinned to the bottom of the nav: troubleshooting tools most projects
	 * never need, grouped in categories (today: the steam_appid.txt buttons). Call from StartupModule.
	 */
	void RegisterAdvancedDashboardPage();
}
