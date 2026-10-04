// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Dashboard/SteamDashboardRegistry.h"

namespace
{
	TArray<FSteamDashboardPage>& GetRegistry()
	{
		static TArray<FSteamDashboardPage> Pages;
		return Pages;
	}
}

namespace SandwichSteam::Editor
{
	void RegisterDashboardPage(FSteamDashboardPage Page)
	{
		TArray<FSteamDashboardPage>& Pages = GetRegistry();
		Pages.RemoveAll([&Page](const FSteamDashboardPage& Existing) { return Existing.Id == Page.Id; });
		Pages.Add(MoveTemp(Page));
	}

	void UnregisterDashboardPage(FName Id)
	{
		GetRegistry().RemoveAll([Id](const FSteamDashboardPage& Existing) { return Existing.Id == Id; });
	}

	TArray<FSteamDashboardPage> GetDashboardPages()
	{
		TArray<FSteamDashboardPage> Pages = GetRegistry();
		// Bottom-pinned pages always come last, so a low Order there never makes one the default (first) page.
		Pages.Sort([](const FSteamDashboardPage& A, const FSteamDashboardPage& B)
		{
			return A.bPinToBottom != B.bPinToBottom ? B.bPinToBottom : A.Order < B.Order;
		});
		return Pages;
	}
}
