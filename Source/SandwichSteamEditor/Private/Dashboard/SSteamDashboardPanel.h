// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;
struct FSteamDashboardPage;

/**
 * Tools > Open Steam Dashboard. Left nav built from the dashboard page registry, the selected page in the center,
 * and a persistent SSteamStatusPanel on the right. Adding a page never requires a change here; see
 * Dashboard/SteamDashboardRegistry.h.
 */
class SSteamDashboardPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSteamDashboardPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeNavButton(int32 PageIndex, const FSteamDashboardPage& Page);

	int32 ActivePage = 0;
	TSharedPtr<SWidgetSwitcher> Pages;
};

namespace SandwichSteam::Editor
{
	/** Registers and removes the Open Steam Dashboard tab (Tools menu entry calls SandwichSteamDashboard). */
	void RegisterDashboardTab();
	void UnregisterDashboardTab();
	void SandwichSteamDashboard();
}
