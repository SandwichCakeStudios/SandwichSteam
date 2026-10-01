// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"

class SWidget;

/**
 * One page of the Steam Dashboard's left nav. A future tool (for example an automatic SteamCMD download/setup tool)
 * is added by registering one more page from its own module, never by editing the dashboard shell. Mirrors the
 * pattern of Debug/SteamDebugSection.h.
 */
struct FSteamDashboardPage
{
	/** Unique id. Registering the same id again replaces the page. */
	FName Id;

	FText Label;
	FSlateIcon Icon;

	/** Lower values are listed first. */
	int32 Order = 0;

	/** Builds the page content. Called once, when the dashboard tab is constructed. */
	TFunction<TSharedRef<SWidget>()> BuildContent;

	/**
	 * Optional. Checked when the dashboard tab is about to close; returning false keeps it open (for example a
	 * running job asks the user to cancel first, the same way its own standalone tab would).
	 */
	TFunction<bool()> CanClose;

	/**
	 * Optional. Polled by the nav every frame, so keep it cheap (read a cached flag). False hides the nav button; the
	 * dashboard then opens on, or moves to, the first visible page. Null = always visible.
	 */
	TFunction<bool()> IsVisible;
};

namespace SandwichSteam::Editor
{
	/** Adds or replaces a dashboard page. */
	void RegisterDashboardPage(FSteamDashboardPage Page);

	/** Removes a page. Call from ShutdownModule for every registered id. */
	void UnregisterDashboardPage(FName Id);

	/** Snapshot of the registered pages, sorted by Order. */
	TArray<FSteamDashboardPage> GetDashboardPages();
}
