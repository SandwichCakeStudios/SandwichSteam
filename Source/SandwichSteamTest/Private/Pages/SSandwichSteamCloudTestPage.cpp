// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamCloudTestPage.h"
#include "Misc/DateTime.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamCloudSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestCloud"

namespace
{
	const TCHAR* const TestSlot = TEXT("TestPanelSlot");
}

void SSandwichSteamCloudTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Save, then Delete local file, then Load: the text must come back from the cloud.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Cloud", "Cloud saves")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamCloudSubsystem* Cloud = GetCloud();
			return Cloud ? SandwichSteamTest::BoolText(Cloud->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Enabled", "Steam Cloud on (account + game)"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamCloudSubsystem* Cloud = GetCloud();
			return Cloud ? SandwichSteamTest::BoolText(Cloud->IsCloudEnabled()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Quota", "Free / total bytes"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamCloudSubsystem* Cloud = GetCloud();
			FSteamCloudQuota Quota;
			return (Cloud && Cloud->GetQuota(Quota))
				? FText::Format(LOCTEXT("QuotaFmt", "{0} / {1}"), FText::AsNumber(Quota.AvailableBytes), FText::AsNumber(Quota.TotalBytes))
				: LOCTEXT("QuotaUnknown", "unknown");
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildSlotText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Save", "Save"), FOnClicked::CreateSP(this, &SSandwichSteamCloudTestPage::OnSave)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Load", "Load"), FOnClicked::CreateSP(this, &SSandwichSteamCloudTestPage::OnLoad)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("DeleteLocal", "Delete local file"), FOnClicked::CreateSP(this, &SSandwichSteamCloudTestPage::OnDeleteLocal)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("DeleteAll", "Delete local + cloud"), FOnClicked::CreateSP(this, &SSandwichSteamCloudTestPage::OnDeleteAll)) ]
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

USteamCloudSubsystem* SSandwichSteamCloudTestPage::GetCloud() const
{
	return USteamCloudSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamCloudTestPage::BuildSlotText() const
{
	const USteamCloudSubsystem* Cloud = GetCloud();
	if (!Cloud)
	{
		return FText::GetEmpty();
	}

	FSteamCloudSaveInfo Local;
	FSteamCloudSaveInfo Remote;
	Cloud->GetSlotInfo(TestSlot, Local, Remote);
	return FText::Format(LOCTEXT("SlotFmt", "Slot {0}: local {1} (saved {2}), cloud {3} (saved {4})"), FText::FromString(TestSlot),
		Local.bValid ? LOCTEXT("Ok", "ok") : LOCTEXT("Missing", "missing or damaged"), FText::AsNumber(Local.UnixTime),
		Remote.bValid ? LOCTEXT("Ok2", "ok") : LOCTEXT("Missing2", "missing or damaged"), FText::AsNumber(Remote.UnixTime));
}

FReply SSandwichSteamCloudTestPage::OnSave()
{
	USteamCloudSubsystem* Cloud = GetCloud();
	if (!Cloud)
	{
		Status = LOCTEXT("NoFeature", "The Cloud feature is not available in this world.");
		return FReply::Handled();
	}

	const FString Text = FString::Printf(TEXT("test panel save %s"), *FDateTime::UtcNow().ToIso8601());
	const FTCHARToUTF8 Utf8(*Text);
	const FSteamCloudSaveResult Saved = Cloud->SaveBytes(TestSlot, TConstArrayView<uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()));
	Status = Saved.Result.IsSuccess()
		? FText::Format(LOCTEXT("Saved", "Saved \"{0}\" (local {1}, cloud {2})."), FText::FromString(Text), SandwichSteamTest::BoolText(Saved.bSavedLocal), SandwichSteamTest::BoolText(Saved.bSavedCloud))
		: FText::Format(LOCTEXT("SaveFailed", "Save failed: {0} (local {1})"), Saved.Result.Message, SandwichSteamTest::BoolText(Saved.bSavedLocal));
	return FReply::Handled();
}

FReply SSandwichSteamCloudTestPage::OnLoad()
{
	USteamCloudSubsystem* Cloud = GetCloud();
	if (!Cloud)
	{
		Status = LOCTEXT("NoFeatureLoad", "The Cloud feature is not available in this world.");
		return FReply::Handled();
	}

	TArray<uint8> Payload;
	const FSteamCloudLoadResult Load = Cloud->LoadBytes(TestSlot, Payload);
	if (Load.Outcome == ESteamCloudLoadOutcome::Loaded)
	{
		const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
		Status = FText::Format(LOCTEXT("Loaded", "Loaded \"{0}\" from the {1} copy."), FText::FromString(FString(Text.Length(), Text.Get())),
			Load.bFromCloud ? LOCTEXT("FromCloud", "cloud") : LOCTEXT("FromLocal", "local"));
	}
	else if (Load.Outcome == ESteamCloudLoadOutcome::NotFound)
	{
		Status = LOCTEXT("NotFound", "Nothing saved in this slot yet.");
	}
	else if (Load.Outcome == ESteamCloudLoadOutcome::Conflict)
	{
		Status = LOCTEXT("Conflict", "Conflict: the copies differ and the policy is Ask.");
	}
	else
	{
		Status = FText::Format(LOCTEXT("LoadFailed", "Load failed: {0}"), Load.Result.Message);
	}
	return FReply::Handled();
}

FReply SSandwichSteamCloudTestPage::OnDeleteLocal()
{
	USteamCloudSubsystem* Cloud = GetCloud();
	const FSteamResult Result = Cloud ? Cloud->DeleteLocalSlot(TestSlot) : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureDel", "The Cloud feature is not available in this world."));
	Status = Result.IsSuccess() ? LOCTEXT("LocalDeleted", "Local file deleted. Load now: it must come back from the cloud.") : Result.Message;
	return FReply::Handled();
}

FReply SSandwichSteamCloudTestPage::OnDeleteAll()
{
	USteamCloudSubsystem* Cloud = GetCloud();
	const FSteamResult Result = Cloud ? Cloud->DeleteSlot(TestSlot) : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureDelAll", "The Cloud feature is not available in this world."));
	Status = Result.IsSuccess() ? LOCTEXT("AllDeleted", "Local file and cloud copy deleted.") : Result.Message;
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
