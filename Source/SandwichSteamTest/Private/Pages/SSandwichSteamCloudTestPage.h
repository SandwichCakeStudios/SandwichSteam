// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USteamCloudSubsystem;

/** Test page for the Cloud feature: save a slot, load it, delete only the local file (the cloud copy must restore it), delete everything, and the state of both copies. */
class SSandwichSteamCloudTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamCloudTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamCloudSubsystem* GetCloud() const;

	FText BuildSlotText() const;

	FReply OnSave();
	FReply OnLoad();
	FReply OnDeleteLocal();
	FReply OnDeleteAll();

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
};
