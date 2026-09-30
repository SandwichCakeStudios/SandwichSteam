// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SandwichSteamTestWidget.generated.h"

/**
 * Test panel for the plugin, built entirely in Slate (no Blueprint needed). One page per feature, selected with the tabs on the left.
 * To add a test page for a new feature: write an SCompoundWidget in Private/Pages and add it to the page list in RebuildWidget().
 */
UCLASS()
class SANDWICHSTEAMTEST_API USandwichSteamTestWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called when the user presses Close. The owner decides what closing means. */
	FSimpleDelegate OnCloseRequested;

protected:
	//~ Begin UWidget
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget

private:
	int32 ActivePage = 0;
};
