// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamVoiceTestPage.h"
#include "Core/SteamResult.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamVoiceSubsystem.h"
#include "Widgets/Notifications/SProgressBar.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestVoice"

void SSandwichSteamVoiceTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Needs the OnlineSubsystemSteam setup with voice (Configure Steam) and, to hear anything, a second machine with a second Steam account in the same game session (Sessions tab: create, host travel, join).");

	const TSharedRef<SVerticalBox> Meters = SNew(SVerticalBox);
	for (int32 Index = 0; Index < MaxMeters; ++Index)
	{
		Meters->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this, Index] { return MeterPlayer[Index].IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(SBox).WidthOverride(330.f)
				[
					SNew(STextBlock).Text_Lambda([this, Index] { return BuildMeterLabel(Index); })
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(14.f)
				[
					SNew(SProgressBar).Percent_Lambda([this, Index] { return MeterLevel[Index]; })
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Voice", "Voice")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamVoiceSubsystem* Voice = GetVoice();
			return Voice ? SandwichSteamTest::BoolText(Voice->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Enabled", "Voice enabled"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamVoiceSubsystem* Voice = GetVoice();
			return Voice ? SandwichSteamTest::BoolText(Voice->IsVoiceEnabled()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Mode", "Mode"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamVoiceSubsystem* Voice = GetVoice();
			if (!Voice)
			{
				return FText::GetEmpty();
			}
			return Voice->GetVoiceMode() == ESteamVoiceMode::PushToTalk ? LOCTEXT("Ptt", "push to talk") : LOCTEXT("Open", "open mic");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("LocalTalking", "You are talking"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamVoiceSubsystem* Voice = GetVoice();
			return Voice ? SandwichSteamTest::BoolText(Voice->IsLocalTalking()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("PttBound", "Push to talk bound"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamVoiceSubsystem* Voice = GetVoice();
			return Voice ? SandwichSteamTest::BoolText(Voice->IsPushToTalkBound()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildTalkingText(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[ Meters ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildStateText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ToggleEnabled", "Enable / disable voice"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnToggleEnabled)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ToggleMode", "Switch mode"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnToggleMode)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ToggleMic", "Microphone on / off (like the PTT key)"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnToggleMicrophone)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("MuteFirst", "Mute first other player"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnMuteFirst)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("UnmuteAll", "Unmute all"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnUnmuteAll)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("VolumeDown", "Volume -0.25"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnVolumeDown)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("VolumeUp", "Volume +0.25"), FOnClicked::CreateSP(this, &SSandwichSteamVoiceTestPage::OnVolumeUp)) ]
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

USteamVoiceSubsystem* SSandwichSteamVoiceTestPage::GetVoice() const
{
	return USteamVoiceSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamVoiceTestPage::BuildStateText() const
{
#if SANDWICHSTEAM_WITH_DEBUG
	const USteamVoiceSubsystem* Voice = GetVoice();
	return Voice ? FText::FromString(Voice->BuildDebugString()) : FText::GetEmpty();
#else
	return FText::GetEmpty();
#endif
}

void SSandwichSteamVoiceTestPage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const USteamVoiceSubsystem* Voice = GetVoice();
	const TArray<FSteamId> Present = (Voice && Voice->IsFeatureActive()) ? Voice->GetPlayersInGame() : TArray<FSteamId>();

	// A player keeps their bar while they stay in the game; a new player takes a free bar.
	for (int32 Index = 0; Index < MaxMeters; ++Index)
	{
		if (MeterPlayer[Index].IsValid() && !Present.Contains(MeterPlayer[Index]))
		{
			MeterPlayer[Index] = FSteamId();
			MeterLevel[Index] = 0.f;
			MeterHeardSeconds[Index] = 0.f;
		}
	}
	for (const FSteamId& Player : Present)
	{
		bool bHasBar = false;
		for (int32 Index = 0; Index < MaxMeters; ++Index)
		{
			bHasBar |= MeterPlayer[Index] == Player;
		}
		for (int32 Index = 0; !bHasBar && Index < MaxMeters; ++Index)
		{
			if (!MeterPlayer[Index].IsValid())
			{
				MeterPlayer[Index] = Player;
				bHasBar = true;
			}
		}
	}

	for (int32 Index = 0; Index < MaxMeters; ++Index)
	{
		const bool bTalking = Voice && MeterPlayer[Index].IsValid() && Voice->IsPlayerTalking(MeterPlayer[Index]);
		MeterLevel[Index] = FMath::FInterpTo(MeterLevel[Index], bTalking ? 1.f : 0.f, InDeltaTime, bTalking ? 30.f : 3.f);
		MeterHeardSeconds[Index] += bTalking ? InDeltaTime : 0.f;
	}
}

FText SSandwichSteamVoiceTestPage::BuildMeterLabel(int32 Index) const
{
	const USteamVoiceSubsystem* Voice = GetVoice();
	const FSteamId Player = MeterPlayer[Index];
	if (!Voice || !Player.IsValid())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Printf(TEXT("%s%s  %s  (%.1f s of voice)%s"), *Player.ToString(), Player == Voice->GetLocalPlayerId() ? TEXT(" (you)") : TEXT(""),
		Voice->IsPlayerTalking(Player) ? TEXT("TALKING") : TEXT("quiet"), MeterHeardSeconds[Index], Voice->IsPlayerMuted(Player) ? TEXT("  muted") : TEXT("")));
}

FText SSandwichSteamVoiceTestPage::BuildTalkingText() const
{
	const USteamVoiceSubsystem* Voice = GetVoice();
	if (!Voice || !Voice->IsFeatureActive())
	{
		return FText::GetEmpty();
	}

	FString Text = FString::Printf(TEXT("Players in the game: %d\n"), Voice->GetPlayersInGame().Num());
	for (const FSteamId& Player : Voice->GetPlayersInGame())
	{
		Text += FString::Printf(TEXT("  %s%s   %s%s\n"), *Player.ToString(), Player == Voice->GetLocalPlayerId() ? TEXT(" (you)") : TEXT(""),
			Voice->IsPlayerTalking(Player) ? TEXT("TALKING") : TEXT("quiet"), Voice->IsPlayerMuted(Player) ? TEXT("   muted") : TEXT(""));
	}
	return FText::FromString(Text.TrimEnd());
}

void SSandwichSteamVoiceTestPage::ReportResult(const FSteamResult& Result, const FText& Done)
{
	Status = Result.IsSuccess() ? Done : FText::Format(LOCTEXT("Failed", "Failed ({0}): {1}"), FText::FromName(Result.ErrorTag.GetTagName()), Result.Message);
}

FReply SSandwichSteamVoiceTestPage::OnToggleEnabled()
{
	USteamVoiceSubsystem* Voice = GetVoice();
	if (!Voice)
	{
		Status = LOCTEXT("NoFeature", "The Voice feature is not available in this world.");
		return FReply::Handled();
	}

	const bool bNew = !Voice->IsVoiceEnabled();
	ReportResult(Voice->SetVoiceEnabled(bNew), bNew ? LOCTEXT("Enabled", "Voice enabled.") : LOCTEXT("Disabled", "Voice disabled."));
	return FReply::Handled();
}

FReply SSandwichSteamVoiceTestPage::OnToggleMode()
{
	USteamVoiceSubsystem* Voice = GetVoice();
	if (!Voice)
	{
		Status = LOCTEXT("NoFeatureMode", "The Voice feature is not available in this world.");
		return FReply::Handled();
	}

	const bool bToOpen = Voice->GetVoiceMode() == ESteamVoiceMode::PushToTalk;
	ReportResult(Voice->SetVoiceMode(bToOpen ? ESteamVoiceMode::OpenMic : ESteamVoiceMode::PushToTalk), bToOpen ? LOCTEXT("NowOpen", "Open mic.") : LOCTEXT("NowPtt", "Push to talk."));
	return FReply::Handled();
}

FReply SSandwichSteamVoiceTestPage::OnToggleMicrophone()
{
	USteamVoiceSubsystem* Voice = GetVoice();
	if (!Voice)
	{
		Status = LOCTEXT("NoFeatureMic", "The Voice feature is not available in this world.");
		return FReply::Handled();
	}

	bMicrophoneToggled = !bMicrophoneToggled;
	ReportResult(bMicrophoneToggled ? Voice->StartTalking() : Voice->StopTalking(), bMicrophoneToggled ? LOCTEXT("MicOn", "Microphone requested (push to talk held).") : LOCTEXT("MicOff", "Microphone released."));
	return FReply::Handled();
}

FReply SSandwichSteamVoiceTestPage::OnMuteFirst()
{
	USteamVoiceSubsystem* Voice = GetVoice();
	if (!Voice)
	{
		Status = LOCTEXT("NoFeatureMute", "The Voice feature is not available in this world.");
		return FReply::Handled();
	}

	for (const FSteamId& Player : Voice->GetPlayersInGame())
	{
		if (Player != Voice->GetLocalPlayerId())
		{
			ReportResult(Voice->MutePlayer(Player), FText::Format(LOCTEXT("Muted", "Muted {0}."), FText::FromString(Player.ToString())));
			return FReply::Handled();
		}
	}

	Status = LOCTEXT("NoOther", "No other player in the game yet. Join the same game session with a second account.");
	return FReply::Handled();
}

FReply SSandwichSteamVoiceTestPage::OnUnmuteAll()
{
	USteamVoiceSubsystem* Voice = GetVoice();
	if (!Voice)
	{
		Status = LOCTEXT("NoFeatureUnmute", "The Voice feature is not available in this world.");
		return FReply::Handled();
	}

	int32 Count = 0;
	for (const FSteamId& Player : Voice->GetMutedPlayers())
	{
		Count += Voice->UnmutePlayer(Player).IsSuccess() ? 1 : 0;
	}
	Status = FText::Format(LOCTEXT("Unmuted", "Removed {0} game mute(s). Players blocked on Steam stay muted."), FText::AsNumber(Count));
	return FReply::Handled();
}

FReply SSandwichSteamVoiceTestPage::OnVolumeDown()
{
	if (USteamVoiceSubsystem* Voice = GetVoice())
	{
		ReportResult(Voice->SetMasterVolume(Voice->GetMasterVolume() - 0.25f), FText::Format(LOCTEXT("VolumeSet", "Master volume {0}."), FText::AsNumber(Voice->GetMasterVolume())));
	}
	return FReply::Handled();
}

FReply SSandwichSteamVoiceTestPage::OnVolumeUp()
{
	if (USteamVoiceSubsystem* Voice = GetVoice())
	{
		ReportResult(Voice->SetMasterVolume(Voice->GetMasterVolume() + 0.25f), FText::Format(LOCTEXT("VolumeSetUp", "Master volume {0}."), FText::AsNumber(Voice->GetMasterVolume())));
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
