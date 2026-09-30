// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

namespace SandwichSteam::Editor
{
	/**
	 * Registers the dashboard's "Steamworks SDK" page: the Steamworks SDK versions found under
	 * Engine/Source/ThirdParty/Steamworks (folders named Steam<VERSION>, for example Steamv164), each with a button
	 * to open its folder, plus a short guide for adding a new SDK version. Call from StartupModule.
	 */
	void RegisterSteamworksSdkDashboardPage();
}
