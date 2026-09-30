// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamScreenshotsTestPage.h"
#include "Core/SteamIdLibrary.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamScreenshotsSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestScreenshots"

void SSandwichSteamScreenshotsTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Turn hook mode on in Project Settings > Sandwich Steam - Screenshots to test the F12 key with the game capturing.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Screenshots", "Screenshots")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
			return Screenshots ? SandwichSteamTest::BoolText(Screenshots->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Hook", "Hook mode"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
			return Screenshots ? SandwichSteamTest::BoolText(Screenshots->IsHooked()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Counters", "Written / ready"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
			return Screenshots
				? FText::Format(LOCTEXT("CountersFmt", "{0} / {1}{2}"), FText::AsNumber(Screenshots->GetWrittenCount()), FText::AsNumber(Screenshots->GetReadyCount()),
					Screenshots->IsCapturePending() ? LOCTEXT("Pending", "  (capturing...)") : FText::GetEmpty())
				: FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Location", "Location / tagged users"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
			return Screenshots
				? FText::Format(LOCTEXT("LocationFmt", "{0} / {1}"), Screenshots->GetLocation().IsEmpty() ? LOCTEXT("None", "none") : FText::FromString(Screenshots->GetLocation()), FText::AsNumber(Screenshots->GetTaggedUserCount()))
				: FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Trigger", "Trigger (Steam captures)"), FOnClicked::CreateSP(this, &SSandwichSteamScreenshotsTestPage::OnTrigger)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Capture", "Capture viewport"), FOnClicked::CreateSP(this, &SSandwichSteamScreenshotsTestPage::OnCapture)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("SetLocation", "Set location"), FOnClicked::CreateSP(this, &SSandwichSteamScreenshotsTestPage::OnLocation)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("TagSelf", "Tag myself"), FOnClicked::CreateSP(this, &SSandwichSteamScreenshotsTestPage::OnTagSelf)) ]
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

USteamScreenshotsSubsystem* SSandwichSteamScreenshotsTestPage::GetScreenshots() const
{
	return USteamScreenshotsSubsystem::Get(WorldContext.Get());
}

FReply SSandwichSteamScreenshotsTestPage::OnTrigger()
{
	USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
	const FSteamResult Result = Screenshots ? Screenshots->TriggerScreenshot() : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeature", "The Screenshots feature is not available in this world."));
	Status = Result.IsSuccess() ? LOCTEXT("Triggered", "Asked Steam to take a screenshot.") : Result.Message;
	return FReply::Handled();
}

FReply SSandwichSteamScreenshotsTestPage::OnCapture()
{
	USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
	const FSteamResult Result = Screenshots ? Screenshots->CaptureViewportToSteam() : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureCapture", "The Screenshots feature is not available in this world."));
	Status = Result.IsSuccess() ? LOCTEXT("Captured", "Capturing the viewport. The counters grow when Steam has the image.") : Result.Message;
	return FReply::Handled();
}

FReply SSandwichSteamScreenshotsTestPage::OnLocation()
{
	if (USteamScreenshotsSubsystem* Screenshots = GetScreenshots())
	{
		Screenshots->SetLocation(TEXT("Sandwich Steam test panel"));
		Status = LOCTEXT("LocationSet", "Location set. It is attached to the next screenshot Steam reports as ready.");
	}
	return FReply::Handled();
}

FReply SSandwichSteamScreenshotsTestPage::OnTagSelf()
{
	USteamScreenshotsSubsystem* Screenshots = GetScreenshots();
	const FSteamResult Result = Screenshots ? Screenshots->TagUser(USteamIdLibrary::GetLocalSteamId(WorldContext.Get())) : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureTag", "The Screenshots feature is not available in this world."));
	Status = Result.IsSuccess() ? LOCTEXT("Tagged", "You are tagged on the next screenshot.") : Result.Message;
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
