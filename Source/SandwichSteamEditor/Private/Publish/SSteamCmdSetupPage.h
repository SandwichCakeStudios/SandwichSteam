// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

namespace SandwichSteam::Editor
{
	/**
	 * Registers the dashboard's "SteamCMD" page: an install status line, a Download SteamCMD button (progress bar
	 * and step text while it runs, Cancel), and the "Your account" details view (USteamPublishUserSettings: SteamCMD
	 * path + Steam account name) moved off the Publish page's Setup tab, since it belongs next to the downloader now
	 * that one exists. On a successful download, offers to fill in the SteamCMD path setting. Call from StartupModule.
	 */
	void RegisterSteamCmdDashboardPage();
}
