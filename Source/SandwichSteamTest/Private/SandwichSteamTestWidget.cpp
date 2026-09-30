// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamTestWidget.h"
#include "Core/SteamCoreSubsystem.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "Pages/SSandwichSteamCloudTestPage.h"
#include "Pages/SSandwichSteamDLCTestPage.h"
#include "Pages/SSandwichSteamFriendsTestPage.h"
#include "Pages/SSandwichSteamInputTestPage.h"
#include "Pages/SSandwichSteamLeaderboardsTestPage.h"
#include "Pages/SSandwichSteamOverlayTestPage.h"
#include "Pages/SSandwichSteamPresenceTestPage.h"
#include "Pages/SSandwichSteamScreenshotsTestPage.h"
#include "Pages/SSandwichSteamSessionsTestPage.h"
#include "Pages/SSandwichSteamStatsTestPage.h"
#include "Pages/SSandwichSteamUserTestPage.h"
#include "Pages/SSandwichSteamUtilityTestPage.h"
#include "Pages/SSandwichSteamVoiceTestPage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestWidget"

TSharedRef<SWidget> USandwichSteamTestWidget::RebuildWidget()
{
	const TWeakObjectPtr<UObject> Context(this);

	// Add a page for every new feature here: title + content.
	struct FPage
	{
		FText Title;
		TSharedRef<SWidget> Content;
	};
	const FPage Pages[] =
	{
		{ LOCTEXT("UserTab", "User"), SNew(SSandwichSteamUserTestPage).WorldContext(Context) },
		{ LOCTEXT("UtilityTab", "Utility"), SNew(SSandwichSteamUtilityTestPage).WorldContext(Context) },
		{ LOCTEXT("OverlayTab", "Overlay"), SNew(SSandwichSteamOverlayTestPage).WorldContext(Context) },
		{ LOCTEXT("StatsTab", "Stats"), SNew(SSandwichSteamStatsTestPage).WorldContext(Context) },
		{ LOCTEXT("LeaderboardsTab", "Leaderboards"), SNew(SSandwichSteamLeaderboardsTestPage).WorldContext(Context) },
		{ LOCTEXT("FriendsTab", "Friends"), SNew(SSandwichSteamFriendsTestPage).WorldContext(Context) },
		{ LOCTEXT("PresenceTab", "Presence"), SNew(SSandwichSteamPresenceTestPage).WorldContext(Context) },
		{ LOCTEXT("CloudTab", "Cloud"), SNew(SSandwichSteamCloudTestPage).WorldContext(Context) },
		{ LOCTEXT("DLCTab", "DLC"), SNew(SSandwichSteamDLCTestPage).WorldContext(Context) },
		{ LOCTEXT("ScreenshotsTab", "Screenshots"), SNew(SSandwichSteamScreenshotsTestPage).WorldContext(Context) },
		{ LOCTEXT("SessionsTab", "Sessions"), SNew(SSandwichSteamSessionsTestPage).WorldContext(Context) },
		{ LOCTEXT("VoiceTab", "Voice"), SNew(SSandwichSteamVoiceTestPage).WorldContext(Context) },
		{ LOCTEXT("InputTab", "Input"), SNew(SSandwichSteamInputTestPage).WorldContext(Context) },
	};

	const TSharedRef<SVerticalBox> Tabs = SNew(SVerticalBox);
	const TSharedRef<SWidgetSwitcher> Switcher = SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this] { return ActivePage; });

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Pages); ++Index)
	{
		Tabs->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(14.f, 6.f))
			.OnClicked_Lambda([this, Index] { ActivePage = Index; return FReply::Handled(); })
			[
				SNew(STextBlock).Text(Pages[Index].Title)
			]
		];
		Switcher->AddSlot()[ Pages[Index].Content ];
	}

	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(40.f))
		[
			SNew(SBox)
			.WidthOverride(760.f)
			.HeightOverride(560.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.92f))
				.Padding(16.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Title", "Sandwich Steam - Test Panel"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
						[
							SNew(STextBlock)
							.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.62f, 0.68f)))
							.Text_Lambda([this]
							{
								const USteamCoreSubsystem* Core = USteamCoreSubsystem::Get(this);
								return Core
									? FText::Format(LOCTEXT("StateFmt", "Steam: {0}"), FText::FromString(LexToString(Core->GetSteamState())))
									: LOCTEXT("NoCore", "Steam core: not available");
							})
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SandwichSteamTest::MakeButton(LOCTEXT("Close", "Close"), FOnClicked::CreateLambda([this]
							{
								OnCloseRequested.ExecuteIfBound();
								return FReply::Handled();
							}))
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f)[ Tabs ]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()[ Switcher ]
						]
					]
				]
			]
		];
}

void USandwichSteamTestWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
}

#undef LOCTEXT_NAMESPACE
