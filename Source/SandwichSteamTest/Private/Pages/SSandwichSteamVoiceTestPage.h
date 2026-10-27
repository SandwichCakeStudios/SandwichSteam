// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Widgets/SCompoundWidget.h"

class USteamVoiceSubsystem;

/**
 * Test page for the Voice feature: enable / disable, mode switch, a toggle for the microphone (same as holding the push to talk key),
 * mic test, microphone level against the activation threshold, talking on channel 1, fake talkers (one machine), mute and unmute the first
 * other player, server rules on the first other player (Server Authority), master volume, and a level bar per player with their transmit channels. The state block is the Steam.Voice.Dump report.
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
	/**
	 * Voice meters: one bar per player in the game (you and up to 8 fake talkers fit). The Steam voice pipeline gives the voice level; the
	 * Online Subsystem voice only says talking / not talking, so the bar is full while someone talks. It rises quickly and falls slowly, so
	 * incoming voice is visible without headphones on the receiving machine.
	 */
	static constexpr int32 MaxMeters = 10;

	/** Level shown as a full bar (loud speech; speech is usually 0.03 to 0.2). */
	static constexpr float FullScaleLevel = 0.2f;

	FText BuildMeterLabel(int32 Index) const;
	USteamVoiceSubsystem* GetVoice() const;

	FText BuildStateText() const;
	FText BuildTalkingText() const;
	FText BuildMicText() const;

	FReply OnToggleEnabled();
	FReply OnToggleAllowed();
	FReply OnTogglePawnless();
	FReply OnToggleMode();
	FReply OnToggleMicrophone();
	FReply OnToggleMicTest();
	FReply OnToggleRadio();
	FReply OnFakeTalkers(int32 Count, int32 Channel);
	FReply OnMuteFirst();
	FReply OnUnmuteAll();
	/** Server rules on the first other player (fake talkers count): bOwnerVoice toggles Can Use Owner Voice, else Can Talk (server mute). */
	FReply OnServerToggleFirst(bool bOwnerVoice);
	/** Dead chat on the first other player: no owner voice, talks and listens on channel 1 only (or back to the default rules). */
	FReply OnServerDeadChatFirst();
	/** The local player may hear no channel (or any channel again): a forbidden channel is dropped. */
	FReply OnServerListenNone();
	/** Owner voice distance: off -> 500 cm -> 2000 cm -> off. */
	FReply OnServerCycleDistance();
	/** Megaphone on the first other player: no distance limit for their voice (or the game's again). */
	FReply OnServerMegaphoneFirst();
	FReply OnServerToggleTopology();
#if SANDWICHSTEAM_WITH_DEBUG
	FReply OnToggleRelayEcho();
#endif
	FReply OnVolumeDown();
	FReply OnVolumeUp();

	void ReportResult(const FSteamResult& Result, const FText& Done);

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;
	bool bMicrophoneToggled = false;

	FSteamId MeterPlayer[MaxMeters];
	float MeterLevel[MaxMeters] = {};
	float MeterHeardSeconds[MaxMeters] = {};
	float MicMeterLevel = 0.f;
};
