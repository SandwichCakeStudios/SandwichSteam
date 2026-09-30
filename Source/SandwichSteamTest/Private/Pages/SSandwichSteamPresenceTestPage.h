// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USteamPresenceSubsystem;

/**
 * Test page for the Presence feature: the first status of the App Definition (its keys get the value "test"), a raw
 * "status" key that works on any App ID, the debounce check (three changes in one frame, the counters show one flush) and clear.
 */
class SSandwichSteamPresenceTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamPresenceTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamPresenceSubsystem* GetPresence() const;

	FText BuildKeysText() const;

	FReply OnSetFirstStatus();
	FReply OnSetByToken();
	FReply OnSetRaw();
	FReply OnBurst();
	FReply OnSetGroup();
	FReply OnClear();
	void ReportResult(const FText& Action, bool bSuccess, const FText& Message);

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
};
