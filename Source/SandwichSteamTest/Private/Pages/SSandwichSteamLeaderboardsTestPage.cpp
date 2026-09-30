// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamLeaderboardsTestPage.h"
#include "Data/SteamAppDefinition.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamLeaderboardsSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestLeaderboards"

void SSandwichSteamLeaderboardsTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Uses the first leaderboard of the Steam App Definition (Project Settings > Plugins > Sandwich Steam).");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Leaderboards", "Leaderboards")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamLeaderboardsSubsystem* Leaderboards = GetLeaderboards();
			return Leaderboards ? SandwichSteamTest::BoolText(Leaderboards->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Board", "Leaderboard"), TAttribute<FText>::CreateLambda([this]
		{
			const FName Name = GetBoardName();
			return Name.IsNone() ? LOCTEXT("NoBoard", "none in the App Definition") : FText::FromName(Name);
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Calls", "Downloads sent / joined"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamLeaderboardsSubsystem* Leaderboards = GetLeaderboards();
			return Leaderboards
				? FText::Format(LOCTEXT("CallsFmt", "{0} / {1}"), FText::AsNumber(Leaderboards->GetDownloadCallCount()), FText::AsNumber(Leaderboards->GetJoinedDownloadCount()))
				: FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildEntriesText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Upload", "Upload random score"), FOnClicked::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::OnUpload)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Top", "Top 10"), FOnClicked::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::OnDownload, ESteamLeaderboardRequestType::Global)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Around", "Around me"), FOnClicked::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::OnDownload, ESteamLeaderboardRequestType::AroundUser)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Friends", "Friends"), FOnClicked::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::OnDownload, ESteamLeaderboardRequestType::Friends)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Twice", "Top 10 twice (de-dup)"), FOnClicked::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::OnDownloadTwice)) ]
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

USteamLeaderboardsSubsystem* SSandwichSteamLeaderboardsTestPage::GetLeaderboards() const
{
	return USteamLeaderboardsSubsystem::Get(WorldContext.Get());
}

FName SSandwichSteamLeaderboardsTestPage::GetBoardName() const
{
	const USteamLeaderboardsSubsystem* Leaderboards = GetLeaderboards();
	const USteamAppDefinition* Definition = Leaderboards ? Leaderboards->GetDefinition() : nullptr;
	return (Definition && !Definition->Leaderboards.IsEmpty()) ? Definition->Leaderboards[0].Name : NAME_None;
}

FText SSandwichSteamLeaderboardsTestPage::BuildEntriesText() const
{
	if (Entries.IsEmpty())
	{
		return LOCTEXT("NoEntries", "No entries loaded yet.");
	}

	FString Text;
	for (const FSteamLeaderboardEntry& Entry : Entries)
	{
		Text += FString::Printf(TEXT("#%d  %s  score %d%s\n"), Entry.Rank, *Entry.SteamId.ToString(), Entry.Score,
			Entry.Details.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("  (%d details)"), Entry.Details.Num()));
	}
	return FText::FromString(Text.TrimEnd());
}

FReply SSandwichSteamLeaderboardsTestPage::OnUpload()
{
	USteamLeaderboardsSubsystem* Leaderboards = GetLeaderboards();
	const FName Name = GetBoardName();
	if (!Leaderboards || Name.IsNone())
	{
		Status = LOCTEXT("UploadNoBoard", "Needs the Leaderboards feature and a leaderboard in the App Definition.");
		return FReply::Handled();
	}

	const FSteamResult Started = Leaderboards->UploadScore(Name, FMath::RandRange(1, 1000), ESteamLeaderboardUploadMethod::KeepBest, TArray<int32>(),
		FSteamLeaderboardUploadDelegate::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::HandleUpload));
	Status = Started.IsSuccess() ? LOCTEXT("Uploading", "Uploading...") : FText::Format(LOCTEXT("UploadFailed", "Upload failed: {0}"), Started.Message);
	return FReply::Handled();
}

FReply SSandwichSteamLeaderboardsTestPage::OnDownload(ESteamLeaderboardRequestType Type)
{
	USteamLeaderboardsSubsystem* Leaderboards = GetLeaderboards();
	const FName Name = GetBoardName();
	if (!Leaderboards || Name.IsNone())
	{
		Status = LOCTEXT("DownloadNoBoard", "Needs the Leaderboards feature and a leaderboard in the App Definition.");
		return FReply::Handled();
	}

	FSteamLeaderboardQuery Query;
	Query.Type = Type;
	Query.RangeStart = Type == ESteamLeaderboardRequestType::AroundUser ? -4 : 1;
	Query.RangeEnd = Type == ESteamLeaderboardRequestType::AroundUser ? 5 : 10;

	const FSteamResult Started = Leaderboards->DownloadEntries(Name, Query, FSteamLeaderboardDownloadDelegate::CreateSP(this, &SSandwichSteamLeaderboardsTestPage::HandleEntries));
	Status = Started.IsSuccess() ? LOCTEXT("Downloading", "Downloading...") : FText::Format(LOCTEXT("DownloadFailed", "Download failed: {0}"), Started.Message);
	return FReply::Handled();
}

FReply SSandwichSteamLeaderboardsTestPage::OnDownloadTwice()
{
	// Two identical requests in the same frame: the counters above must show +1 sent and +1 joined.
	OnDownload(ESteamLeaderboardRequestType::Global);
	OnDownload(ESteamLeaderboardRequestType::Global);
	return FReply::Handled();
}

void SSandwichSteamLeaderboardsTestPage::HandleUpload(const FSteamResult& Result, const FSteamLeaderboardUploadResult& Upload)
{
	Status = Result.IsSuccess()
		? FText::Format(LOCTEXT("UploadOk", "Uploaded {0}: {1}, rank {2} -> {3}"), FText::AsNumber(Upload.Score),
			Upload.bScoreChanged ? LOCTEXT("Changed", "score changed") : LOCTEXT("Kept", "better score kept"), FText::AsNumber(Upload.PreviousRank), FText::AsNumber(Upload.NewRank))
		: FText::Format(LOCTEXT("UploadError", "Upload failed: {0}"), Result.Message);
}

void SSandwichSteamLeaderboardsTestPage::HandleEntries(const FSteamResult& Result, const TArray<FSteamLeaderboardEntry>& NewEntries)
{
	if (Result.IsSuccess())
	{
		Entries = NewEntries;
		Status = FText::Format(LOCTEXT("Loaded", "{0} entrie(s) loaded."), FText::AsNumber(Entries.Num()));
	}
	else
	{
		Status = FText::Format(LOCTEXT("LoadError", "Download failed: {0}"), Result.Message);
	}
}

#undef LOCTEXT_NAMESPACE
