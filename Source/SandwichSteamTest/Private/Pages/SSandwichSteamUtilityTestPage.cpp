// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamUtilityTestPage.h"
#include "Features/Utility/SteamUtilitySubsystem.h"
#include "Pages/SandwichSteamTestSlate.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestUtility"

void SSandwichSteamUtilityTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready.");

	// Rows re-read the subsystem every frame, so they always show live values.
	auto MakeLiveRow = [this](const FText& Label, TFunction<FText(const USteamUtilitySubsystem&)> Getter)
	{
		return SandwichSteamTest::MakeRow(Label, TAttribute<FText>::CreateLambda([this, Getter]
		{
			const USteamUtilitySubsystem* Utility = GetUtility();
			return Utility ? Getter(*Utility) : FText::GetEmpty();
		}));
	};

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Facts", "Steam facts")) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("Active", "Feature active"), [](const USteamUtilitySubsystem& U) { return SandwichSteamTest::BoolText(U.IsFeatureActive()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("AppId", "App ID"), [](const USteamUtilitySubsystem& U) { return FText::AsNumber(U.GetAppId(), &FNumberFormattingOptions::DefaultNoGrouping()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("Country", "IP country"), [](const USteamUtilitySubsystem& U) { return FText::FromString(U.GetIpCountry()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("UiLang", "Steam UI language"), [](const USteamUtilitySubsystem& U) { return FText::FromString(U.GetSteamUiLanguage()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("GameLang", "Game language"), [](const USteamUtilitySubsystem& U) { return FText::FromString(U.GetCurrentGameLanguage()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("ServerTime", "Server time (UTC)"), [](const USteamUtilitySubsystem& U) { return FText::FromString(U.GetServerRealTime().ToString()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("Active2", "Seconds since app active"), [](const USteamUtilitySubsystem& U) { return FText::AsNumber(U.GetSecondsSinceAppActive()); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeLiveRow(LOCTEXT("Deck", "Steam Deck / Big Picture"), [](const USteamUtilitySubsystem& U) { return FText::Format(LOCTEXT("DeckFmt", "{0} / {1}"), SandwichSteamTest::BoolText(U.IsRunningOnSteamDeck()), SandwichSteamTest::BoolText(U.IsBigPictureMode())); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Input", "On-screen text input")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ShowInput", "Show gamepad text input"), FOnClicked::CreateSP(this, &SSandwichSteamUtilityTestPage::OnShowTextInput)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)))
			.Text_Lambda([this] { return Status; })
		]
	];
}

USteamUtilitySubsystem* SSandwichSteamUtilityTestPage::GetUtility() const
{
	return USteamUtilitySubsystem::Get(WorldContext.Get());
}

FReply SSandwichSteamUtilityTestPage::OnShowTextInput()
{
	USteamUtilitySubsystem* Utility = GetUtility();
	if (!Utility)
	{
		Status = LOCTEXT("NoUtility", "Utility feature is not available.");
		return FReply::Handled();
	}

	const FSteamResult Result = Utility->ShowGamepadTextInput(ESteamGamepadTextInputMode::Normal, ESteamGamepadTextLineMode::SingleLine,
		LOCTEXT("InputPrompt", "Sandwich Steam test input"), 64, FString(), FSteamTextInputDelegate::CreateSP(this, &SSandwichSteamUtilityTestPage::HandleTextInput));

	Status = Result.IsSuccess()
		? LOCTEXT("InputOpen", "Text input open...")
		: FText::Format(LOCTEXT("InputFailed", "Could not show the text input: {0}"), Result.Message);
	return FReply::Handled();
}

void SSandwichSteamUtilityTestPage::HandleTextInput(bool bSubmitted, const FString& Text)
{
	Status = bSubmitted
		? FText::Format(LOCTEXT("InputSubmitted", "Submitted: \"{0}\""), FText::FromString(Text))
		: LOCTEXT("InputCancelled", "Text input cancelled.");
}

#undef LOCTEXT_NAMESPACE
