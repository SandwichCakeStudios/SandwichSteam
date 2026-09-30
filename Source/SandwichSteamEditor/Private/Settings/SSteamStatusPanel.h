// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Validation/SteamProjectValidator.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
struct FPropertyChangedEvent;

/**
 * Live validator checks (green ok, blue info, orange warning, red error) with inline Fix buttons, plus Re-check,
 * Show in Message Log, Configure Steam... and the two steam_appid.txt buttons. Used by the Project Settings page
 * and the Steam Dashboard's right column, so there is one implementation, not two copies to keep in sync.
 */
class SSteamStatusPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSteamStatusPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SSteamStatusPanel() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void HandleObjectChanged(UObject* Object, FPropertyChangedEvent& Event);
	void RequestRefresh();
	void Refresh();

	TSharedPtr<SVerticalBox> Rows;
	TArray<FSteamValidationCheck> Checks;
	FDelegateHandle ObjectChangedHandle;
	FDelegateHandle DefinitionChangedHandle;
	bool bRefreshRequested = false;
};
