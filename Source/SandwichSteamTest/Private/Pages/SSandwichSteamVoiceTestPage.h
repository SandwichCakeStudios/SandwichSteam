// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Widgets/SCompoundWidget.h"

class USteamVoiceSubsystem;

/**
 * Test page for the Voice feature (needs two machines and two Steam accounts in the same game session; see Systems/Voice.md):
 * enable / disable, mode switch, a toggle for the microphone (same as holding the push to talk key), mute and unmute the first other
 * player, master volume, and a live list of who talks. The state block is the Steam.Voice.Dump report.
 */
class SSandwichSteamVoiceTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamVoiceTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

private:
	/** Talking meters: one bar per player in the game. IOnlineVoice only says talking / not talking (no level), so the bar rises quickly while
	 *  someone talks and falls slowly; it lets you see incoming voice without headphones on the receiving machine. */
	static constexpr int32 MaxMeters = 4;

	FText BuildMeterLabel(int32 Index) const;
	USteamVoiceSubsystem* GetVoice() const;

	FText BuildStateText() const;
	FText BuildTalkingText() const;

	FReply OnToggleEnabled();
	FReply OnToggleMode();
	FReply OnToggleMicrophone();
	FReply OnMuteFirst();
	FReply OnUnmuteAll();
	FReply OnVolumeDown();
	FReply OnVolumeUp();

	void ReportResult(const FSteamResult& Result, const FText& Done);

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
	bool bMicrophoneToggled = false;

	FSteamId MeterPlayer[MaxMeters];
	float MeterLevel[MaxMeters] = {};
	float MeterHeardSeconds[MaxMeters] = {};
};
