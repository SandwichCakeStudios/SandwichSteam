// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamPresenceTestPage.h"
#include "Data/SteamAppDefinition.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamPresenceLibrary.h"
#include "SteamPresenceSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestPresence"

void SSandwichSteamPresenceTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Look at your Steam profile from a second account, or use Steam.Presence.Friend <SteamID64>.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Presence", "Rich Presence")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamPresenceSubsystem* Presence = GetPresence();
			return Presence ? SandwichSteamTest::BoolText(Presence->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Statuses", "Statuses in the definition"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamPresenceSubsystem* Presence = GetPresence();
			const USteamAppDefinition* Definition = Presence ? Presence->GetDefinition() : nullptr;
			return Definition ? FText::AsNumber(Definition->Presence.Num()) : LOCTEXT("NoDefinition", "no App Definition");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Flushes", "Flushes / Steam calls"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamPresenceSubsystem* Presence = GetPresence();
			return Presence
				? FText::Format(LOCTEXT("FlushesFmt", "{0} / {1}"), FText::AsNumber(Presence->GetFlushCount()), FText::AsNumber(Presence->GetSteamCallCount()))
				: FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildKeysText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("SetFirst", "Set first status"), FOnClicked::CreateSP(this, &SSandwichSteamPresenceTestPage::OnSetFirstStatus)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("SetByToken", "Set by token"), FOnClicked::CreateSP(this, &SSandwichSteamPresenceTestPage::OnSetByToken)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("SetRaw", "Set raw \"status\""), FOnClicked::CreateSP(this, &SSandwichSteamPresenceTestPage::OnSetRaw)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Burst", "3 changes in 1 frame"), FOnClicked::CreateSP(this, &SSandwichSteamPresenceTestPage::OnBurst)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Group", "Set group"), FOnClicked::CreateSP(this, &SSandwichSteamPresenceTestPage::OnSetGroup)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Clear", "Clear"), FOnClicked::CreateSP(this, &SSandwichSteamPresenceTestPage::OnClear)) ]
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

USteamPresenceSubsystem* SSandwichSteamPresenceTestPage::GetPresence() const
{
	return USteamPresenceSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamPresenceTestPage::BuildKeysText() const
{
	const USteamPresenceSubsystem* Presence = GetPresence();
	if (!Presence)
	{
		return FText::GetEmpty();
	}

	// The applied keys are in the debug report; show its key lines here.
	FString Text;
#if SANDWICHSTEAM_WITH_DEBUG
	TArray<FString> Lines;
	Presence->BuildDebugString().ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("  Keys:")) || Line.StartsWith(TEXT("    ")))
		{
			Text += Line.TrimStart() + TEXT("\n");
		}
	}
#endif
	return Text.IsEmpty() ? LOCTEXT("NoKeys", "No keys set.") : FText::FromString(Text.TrimEnd());
}

void SSandwichSteamPresenceTestPage::ReportResult(const FText& Action, bool bSuccess, const FText& Message)
{
	Status = bSuccess
		? FText::Format(LOCTEXT("Staged", "{0}: staged, sent with the next frame."), Action)
		: FText::Format(LOCTEXT("Failed", "{0} failed: {1}"), Action, Message);
}

FReply SSandwichSteamPresenceTestPage::OnSetFirstStatus()
{
	USteamPresenceSubsystem* Presence = GetPresence();
	const USteamAppDefinition* Definition = Presence ? Presence->GetDefinition() : nullptr;
	if (!Presence || !Definition || Definition->Presence.IsEmpty())
	{
		Status = LOCTEXT("NoStatus", "Needs the Presence feature and a status in the Steam App Definition (Presence array).");
		return FReply::Handled();
	}

	const FSteamPresenceDef& Def = Definition->Presence[0];
	TMap<FName, FString> Args;
	for (const FName Key : Def.ExtraKeys)
	{
		Args.Add(Key, TEXT("test"));
	}

	const FSteamResult Result = Presence->SetPresenceByTag(Def.Tag, Args);
	ReportResult(FText::Format(LOCTEXT("SetStatus", "Status {0}"), FText::FromString(Def.Token)), Result.IsSuccess(), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamPresenceTestPage::OnSetByToken()
{
	// Through the Blueprint library (the By Token node). Without a status row a made-up token is used: Steam takes it, friends see nothing.
	const USteamPresenceSubsystem* Presence = GetPresence();
	const USteamAppDefinition* Definition = Presence ? Presence->GetDefinition() : nullptr;
	const FString Token = (Definition && !Definition->Presence.IsEmpty()) ? Definition->Presence[0].Token : FString(TEXT("#OST_TestStatus"));

	const FSteamResult Result = USteamPresenceLibrary::SetSteamPresenceByToken(WorldContext.Get(), Token, TMap<FName, FString>());
	const FSteamResult Bad = USteamPresenceLibrary::SetSteamPresenceByToken(WorldContext.Get(), TEXT("NoHashToken"), TMap<FName, FString>());

	Status = FText::FromString(FString::Printf(TEXT("By token %s: %s\nToken without #: %s"), *Token,
		Result.IsSuccess() ? TEXT("staged") : *Result.Message.ToString(),
		Bad.IsSuccess() ? TEXT("SUCCEEDED (wrong!)") : *Bad.Message.ToString()));
	return FReply::Handled();
}

FReply SSandwichSteamPresenceTestPage::OnSetRaw()
{
	USteamPresenceSubsystem* Presence = GetPresence();
	if (!Presence)
	{
		Status = LOCTEXT("NoFeature", "The Presence feature is not available in this world.");
		return FReply::Handled();
	}

	const FSteamResult Result = Presence->SetPresenceValue(FName(TEXT("status")), TEXT("Testing Sandwich Steam"));
	ReportResult(LOCTEXT("SetRawAction", "Raw status"), Result.IsSuccess(), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamPresenceTestPage::OnBurst()
{
	USteamPresenceSubsystem* Presence = GetPresence();
	if (!Presence)
	{
		Status = LOCTEXT("NoFeatureBurst", "The Presence feature is not available in this world.");
		return FReply::Handled();
	}

	// Three changes of the same key in one frame: Steam must see one call with the last value.
	Presence->SetPresenceValue(FName(TEXT("burst")), TEXT("one"));
	Presence->SetPresenceValue(FName(TEXT("burst")), TEXT("two"));
	const FSteamResult Result = Presence->SetPresenceValue(FName(TEXT("burst")), TEXT("three"));
	Status = Result.IsSuccess()
		? LOCTEXT("BurstDone", "Set burst = one, two, three in one frame. The counters above must grow by one flush and one Steam call.")
		: FText::Format(LOCTEXT("BurstFailed", "Failed: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamPresenceTestPage::OnSetGroup()
{
	USteamPresenceSubsystem* Presence = GetPresence();
	if (!Presence)
	{
		Status = LOCTEXT("NoFeatureGroup", "The Presence feature is not available in this world.");
		return FReply::Handled();
	}

	const FSteamResult Result = Presence->SetGroup(TEXT("test-party"), 2);
	ReportResult(LOCTEXT("SetGroupAction", "Group"), Result.IsSuccess(), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamPresenceTestPage::OnClear()
{
	USteamPresenceSubsystem* Presence = GetPresence();
	if (!Presence)
	{
		Status = LOCTEXT("NoFeatureClear", "The Presence feature is not available in this world.");
		return FReply::Handled();
	}

	const FSteamResult Result = Presence->ClearPresence();
	ReportResult(LOCTEXT("ClearAction", "Clear"), Result.IsSuccess(), Result.Message);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
