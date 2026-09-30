// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "Widgets/SCompoundWidget.h"

class USteamUtilitySubsystem;

/** Test page for the Utility feature: live Steam facts and the on-screen text input. */
class SSandwichSteamUtilityTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamUtilityTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamUtilitySubsystem* GetUtility() const;

	FReply OnShowTextInput();
	void HandleTextInput(bool bSubmitted, const FString& Text);

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
};
