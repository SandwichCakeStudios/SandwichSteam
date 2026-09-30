// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;
struct FSlateFontInfo;

/**
 * Slate style set of the Sandwich Steam editor UI ("SandwichSteamStyle"). Icons come from the plugin's Resources folder;
 * everything generic (buttons, text, colors) is taken from FAppStyle so the tools follow the editor theme.
 *
 * Brushes:
 *   SandwichSteam.Icon16 / Icon20 / Icon40   plugin icon, for menus, tabs and the settings header
 *   SandwichSteam.Status.Ok / Warning / Error / Info   status dots of the validator
 */
class FSteamToolStyle
{
public:
	static void Register();
	static void Unregister();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	/** Base's own size scaled by Scale (rounded to the nearest point), same face/weight. Used for dashboard headings/text that read larger than the editor default. */
	static FSlateFontInfo ScaleFont(const FSlateFontInfo& Base, float Scale);

private:
	static TSharedRef<FSlateStyleSet> Create();

	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
