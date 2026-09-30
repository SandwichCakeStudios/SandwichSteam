// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamOverlayTestPage.h"
#include "Core/SteamToolSettings.h"
#include "Features/Overlay/SteamOverlaySubsystem.h"
#include "Features/User/SteamUserSubsystem.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestOverlay"

void SSandwichSteamOverlayTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. The overlay only renders in Standalone / packaged games started with the Steam client running.");

	const TSharedRef<SWrapBox> Buttons = SNew(SWrapBox).UseAllottedSize(true);

	const struct FDialogButton { ESteamOverlayDialog Dialog; FText Label; } DialogButtons[] =
	{
		{ ESteamOverlayDialog::Friends, LOCTEXT("Friends", "Friends") },
		{ ESteamOverlayDialog::Community, LOCTEXT("Community", "Community") },
		{ ESteamOverlayDialog::Players, LOCTEXT("Players", "Players") },
		{ ESteamOverlayDialog::Settings, LOCTEXT("Settings", "Settings") },
		{ ESteamOverlayDialog::OfficialGameGroup, LOCTEXT("Group", "Official group") },
		{ ESteamOverlayDialog::Stats, LOCTEXT("Stats", "Stats") },
		{ ESteamOverlayDialog::Achievements, LOCTEXT("Achievements", "Achievements") },
		{ ESteamOverlayDialog::Store, LOCTEXT("Store", "Store (this game)") },
	};

	for (const FDialogButton& Entry : DialogButtons)
	{
		Buttons->AddSlot().Padding(0.f, 0.f, 6.f, 6.f)[ SandwichSteamTest::MakeButton(Entry.Label, FOnClicked::CreateSP(this, &SSandwichSteamOverlayTestPage::OnOpenDialog, Entry.Dialog)) ];
	}
	Buttons->AddSlot().Padding(0.f, 0.f, 6.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Profile", "My profile"), FOnClicked::CreateSP(this, &SSandwichSteamOverlayTestPage::OnOpenMyProfile)) ];
	Buttons->AddSlot().Padding(0.f, 0.f, 6.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Web", "Web page"), FOnClicked::CreateSP(this, &SSandwichSteamOverlayTestPage::OnOpenWebPage)) ];

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("State", "State")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamOverlaySubsystem* Overlay = GetOverlay();
				return SandwichSteamTest::BoolText(Overlay && Overlay->IsFeatureActive());
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Enabled", "Overlay enabled"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamOverlaySubsystem* Overlay = GetOverlay();
				return SandwichSteamTest::BoolText(Overlay && Overlay->IsOverlayEnabled());
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Open", "Overlay open"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamOverlaySubsystem* Overlay = GetOverlay();
				return SandwichSteamTest::BoolText(Overlay && Overlay->IsOverlayActive());
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("AutoPause", "Auto pause (setting)"), TAttribute<FText>::CreateLambda([]
			{
				const USteamToolSettings* Settings = USteamToolSettings::Get();
				return SandwichSteamTest::BoolText(Settings && Settings->bAutoPauseOnOverlay);
			}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Pages", "Open a page")) ]
		+ SVerticalBox::Slot().AutoHeight()[ Buttons ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)))
			.Text_Lambda([this] { return Status; })
		]
	];
}

USteamOverlaySubsystem* SSandwichSteamOverlayTestPage::GetOverlay() const
{
	return USteamOverlaySubsystem::Get(WorldContext.Get());
}

void SSandwichSteamOverlayTestPage::SetResultStatus(const FText& Action, const FSteamResult& Result)
{
	Status = Result.IsSuccess()
		? FText::Format(LOCTEXT("Opened", "{0}: ok"), Action)
		: FText::Format(LOCTEXT("OpenFailed", "{0}: {1}"), Action, Result.Message);
}

FReply SSandwichSteamOverlayTestPage::OnOpenDialog(ESteamOverlayDialog Dialog)
{
	if (USteamOverlaySubsystem* Overlay = GetOverlay())
	{
		const UEnum* Enum = StaticEnum<ESteamOverlayDialog>();
		SetResultStatus(Enum->GetDisplayNameTextByValue(static_cast<int64>(Dialog)), Overlay->OpenDialog(Dialog));
	}
	return FReply::Handled();
}

FReply SSandwichSteamOverlayTestPage::OnOpenMyProfile()
{
	USteamOverlaySubsystem* Overlay = GetOverlay();
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContext.Get());
	if (Overlay && User)
	{
		SetResultStatus(LOCTEXT("ProfileAction", "My profile"), Overlay->OpenTargetDialog(ESteamOverlayDialog::UserProfile, User->GetLocalSteamId()));
	}
	return FReply::Handled();
}

FReply SSandwichSteamOverlayTestPage::OnOpenWebPage()
{
	if (USteamOverlaySubsystem* Overlay = GetOverlay())
	{
		SetResultStatus(LOCTEXT("WebAction", "Web page"), Overlay->OpenWebPage(TEXT("https://partner.steamgames.com/doc/sdk/api"), false));
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
