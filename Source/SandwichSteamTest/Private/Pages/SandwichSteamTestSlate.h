// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

/** Small Slate building blocks shared by the test pages. */
namespace SandwichSteamTest
{
	inline FText BoolText(bool bValue)
	{
		return bValue ? NSLOCTEXT("SandwichSteamTest", "Yes", "yes") : NSLOCTEXT("SandwichSteamTest", "No", "no");
	}

	/** "Label   value" row. Value is re-evaluated every frame, so it always shows the live state. */
	inline TSharedRef<SWidget> MakeRow(const FText& Label, TAttribute<FText> Value)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 2.f, 12.f, 2.f)
			[
				SNew(SBox)
				.WidthOverride(190.f)
				[
					SNew(STextBlock)
					.Text(Label)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.62f, 0.68f)))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(0.f, 2.f)
			[
				SNew(STextBlock)
				.Text(Value)
			];
	}

	inline TSharedRef<SWidget> MakeSectionTitle(const FText& Title)
	{
		return SNew(STextBlock)
			.Text(Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.75f, 1.f)));
	}

	inline TSharedRef<SWidget> MakeButton(const FText& Label, FOnClicked OnClicked)
	{
		return SNew(SButton)
			.OnClicked(OnClicked)
			.ContentPadding(FMargin(10.f, 4.f))
			[
				SNew(STextBlock).Text(Label)
			];
	}
}
