// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USteamScreenshotsSubsystem;

/** Test page for the Screenshots feature: trigger (Steam captures), capture (the game captures the viewport), location and tagging yourself. Check the Steam screenshot library (Shift+Tab, Screenshots). */
class SSandwichSteamScreenshotsTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamScreenshotsTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamScreenshotsSubsystem* GetScreenshots() const;

	FReply OnTrigger();
	FReply OnCapture();
	FReply OnLocation();
	FReply OnTagSelf();

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
};
