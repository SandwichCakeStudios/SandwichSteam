// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USteamDLCSubsystem;

/** Test page for the DLC feature: the rows of the App Definition and every DLC Steam lists with owned and installed state, install / uninstall / store page of the first row. */
class SSandwichSteamDLCTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamDLCTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamDLCSubsystem* GetDLC() const;

	FText BuildListText() const;

	FReply OnInstall();
	FReply OnUninstall();
	FReply OnStore();
	FReply OnByAppIdCheck();

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
};
