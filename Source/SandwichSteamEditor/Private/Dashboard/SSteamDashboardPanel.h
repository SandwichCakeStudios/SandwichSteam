// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;
struct FSteamDashboardPage;

/**
 * Tools > Steam Dashboard. Left nav built from the dashboard page registry, the selected page in the center,
 * and a persistent SSteamStatusPanel on the right. Adding a page never requires a change here; see
 * Dashboard/SteamDashboardRegistry.h.
 */
class SSteamDashboardPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSteamDashboardPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows the page with this id. False when no such page is registered or it is hidden. */
	bool SelectPage(FName PageId);

private:
	TSharedRef<SWidget> MakeNavButton(int32 PageIndex, const FSteamDashboardPage& Page);

	bool IsPageVisible(int32 PageIndex) const;

	/** ActivePage, or the first visible page when ActivePage is hidden. */
	int32 GetDisplayedPage() const;

	int32 ActivePage = 0;
	TArray<FName> PageIds;
	TArray<TFunction<bool()>> PageVisibility;
	TSharedPtr<SWidgetSwitcher> Pages;
};

namespace SandwichSteam::Editor
{
	/** Registers and removes the Steam Dashboard tab (Tools menu entry calls SandwichSteamDashboard). */
	void RegisterDashboardTab();
	void UnregisterDashboardTab();
	void SandwichSteamDashboard();

	/** Opens (or brings to front) the dashboard and shows the page with this id, e.g. "Publish". */
	void OpenDashboardPage(FName PageId);
}
