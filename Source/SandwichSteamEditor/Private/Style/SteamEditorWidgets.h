// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/Notifications/SNotificationList.h"

class SWidget;

/**
 * Small Slate helpers shared by the Sandwich Steam editor pages and actions. Kept in one place (not copied per file) so the
 * pages look and notify the same way.
 */
namespace SandwichSteam::Editor
{
	/** Body text of the dashboard pages: the editor's regular font scaled by 1.25. */
	FSlateFontInfo BodyFont();

	/** Shows an editor toast with a completion state (success, fail, pending). */
	void Notify(const FText& Text, SNotificationItem::ECompletionState State, float Duration = 8.f);

	/** Toast for a file the editor wrote. On success with a path it adds a "Show file" link that opens the folder. */
	void NotifyFileWritten(const FText& Text, const FString& FilePath, bool bSuccess);

	/** Plain button with a body font label. */
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick, TAttribute<bool> bEnabled = true);
}
