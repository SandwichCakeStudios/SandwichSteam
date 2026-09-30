// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "Features/Overlay/SteamOverlayTypes.h"
#include "Widgets/SCompoundWidget.h"

class USteamOverlaySubsystem;

/** Test page for the Overlay feature: state and one button per overlay page. */
class SSandwichSteamOverlayTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamOverlayTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamOverlaySubsystem* GetOverlay() const;

	FReply OnOpenDialog(ESteamOverlayDialog Dialog);
	FReply OnOpenMyProfile();
	FReply OnOpenWebPage();
	void SetResultStatus(const FText& Action, const FSteamResult& Result);

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
};
