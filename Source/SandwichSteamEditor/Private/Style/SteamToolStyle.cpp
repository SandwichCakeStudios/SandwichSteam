// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Style/SteamToolStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"

TSharedPtr<FSlateStyleSet> FSteamToolStyle::StyleInstance = nullptr;

void FSteamToolStyle::Register()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FSteamToolStyle::Unregister()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

const ISlateStyle& FSteamToolStyle::Get()
{
	Register();
	return *StyleInstance;
}

FName FSteamToolStyle::GetStyleSetName()
{
	return TEXT("SandwichSteamStyle");
}

TSharedRef<FSlateStyleSet> FSteamToolStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SandwichSteam"));
	if (Plugin.IsValid())
	{
		const FString Resources = Plugin->GetBaseDir() / TEXT("Resources");
		Style->SetContentRoot(Resources);

		const FString IconPath = Resources / TEXT("Icon128.png");
		Style->Set("SandwichSteam.Icon16", new FSlateImageBrush(IconPath, FVector2f(16.f, 16.f)));
		Style->Set("SandwichSteam.Icon20", new FSlateImageBrush(IconPath, FVector2f(20.f, 20.f)));
		Style->Set("SandwichSteam.Icon40", new FSlateImageBrush(IconPath, FVector2f(40.f, 40.f)));

		const FString PublishIconPath = Resources / TEXT("SteamPublishIcon.png");
		Style->Set("SandwichSteam.Publish16", new FSlateImageBrush(PublishIconPath, FVector2f(16.f, 16.f)));
		Style->Set("SandwichSteam.Publish20", new FSlateImageBrush(PublishIconPath, FVector2f(20.f, 20.f)));
		Style->Set("SandwichSteam.Publish40", new FSlateImageBrush(PublishIconPath, FVector2f(40.f, 40.f)));
	}

	// Status dots. Literal colors stay readable in both editor themes.
	Style->Set("SandwichSteam.Status.Ok", new FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.20f, 0.72f, 0.35f)), 5.0f));
	Style->Set("SandwichSteam.Status.Warning", new FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.95f, 0.62f, 0.10f)), 5.0f));
	Style->Set("SandwichSteam.Status.Error", new FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.90f, 0.25f, 0.25f)), 5.0f));
	Style->Set("SandwichSteam.Status.Info", new FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.40f, 0.60f, 0.90f)), 5.0f));
	Style->Set("SandwichSteam.Status.Idle", new FSlateRoundedBoxBrush(FSlateColor(FLinearColor(0.45f, 0.45f, 0.48f)), 5.0f));

	// Card behind the status strip and the feature list.
	Style->Set("SandwichSteam.Card", new FSlateRoundedBoxBrush(FStyleColors::Recessed, 4.0f));

	return Style;
}

FSlateFontInfo FSteamToolStyle::ScaleFont(const FSlateFontInfo& Base, float Scale)
{
	FSlateFontInfo Font = Base;
	Font.Size = FMath::RoundToInt(Font.Size * Scale);
	return Font;
}
