// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Core/SteamResult.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class USteamInputSubsystem;

/**
 * Test page for the Input feature (Steam Input): controllers and their type, the active action set and layers, cycling the action set, toggling the
 * first layer, rumble, the light bar, Steam's binding panel and the glyph of the first action of the active set. The state block is the Steam.Input.Dump
 * report. Needs a controller and an action file (see Systems/SteamInput.md).
 */
class SSandwichSteamInputTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamInputTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamInputSubsystem* GetInput() const;

	FText BuildStateText() const;
	FText BuildControllersText() const;

	FReply OnNextSet();
	FReply OnNextSetByName();
	FReply OnToggleLayer();
	FReply OnRumble();
	FReply OnStopRumble();
	FReply OnLedRed();
	FReply OnLedReset();
	FReply OnBindingPanel();
	FReply OnShowGlyph();

	void ReportResult(const FSteamResult& Result, const FText& Done);

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
	FText GlyphLabel;
	FSlateBrush GlyphBrush;
	TStrongObjectPtr<UTexture2D> GlyphTexture;
};
