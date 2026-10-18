// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Core/SteamId.h"
#include "DSP/MultithreadedPatching.h"
#include "SteamVoiceTypes.h"

#include <atomic>

#include "SteamVoiceComponent.generated.h"

class APawn;

/**
 * A Steam voice speaker: plays Steam voice through a normal Unreal audio component. The speaker says which voices it plays (Source);
 * attenuation, spatialization, sound class, submix sends, source effects and concurrency are the component's usual audio settings.
 *
 * - Owner (default): plays the player who owns the pawn this component belongs to. Put it on the pawn (or on an actor the pawn owns)
 *   and give it an attenuation for proximity voice. A talker's pawn without one gets one added (Auto Add Voice Component, Voice settings).
 * - Channel: plays everyone transmitting on Channel (walkie, radio, PA), on any actor.
 *
 * The plugin starts the sound when voice arrives and stops it after a second of silence, so a silent speaker costs no mixer source.
 * Activate / Deactivate switch the speaker on and off (walkie off); Auto Activate off means it starts switched off. A switched off Owner
 * speaker keeps its pawn silent: no speaker is added in its place.
 * Any speaker without a sound class gets the Voice Sound Class of the settings.
 *
 * The voice worker decodes each talker (and mixes each channel) and pushes the audio into a patch; this component reads it on the audio
 * render thread and keeps it buffered: Playout Delay for an Owner speaker, a short buffer for a Channel speaker, whose mix already absorbs
 * the network jitter (fades in after a gap, fades out on an underrun). After a gap it starts on the first audio with lead silence, so
 * that audio plays exactly the buffer time after it arrived.
 */
UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent, DisplayName = "Steam Voice"))
class SANDWICHSTEAMVOICE_API USteamVoiceComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	USteamVoiceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Which voices this speaker plays. Set it in the editor; it is fixed once the speaker began play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam Voice", meta = (ToolTip = "Owner: the player who owns this pawn (their mouth; give it an attenuation for proximity voice). Channel: everyone transmitting on Channel, mixed (walkie, radio, PA); works on any actor."))
	ESteamVoiceSpeakerSource Source = ESteamVoiceSpeakerSource::Owner;

	/** The channel a Channel speaker plays. Change it at runtime with Set Channel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam Voice", meta = (EditCondition = "Source == ESteamVoiceSpeakerSource::Channel", EditConditionHides, ToolTip = "The channel this speaker plays (a Steam.Voice.Channel tag or an id). Change it at runtime with Set Channel (walkie dial)."))
	FSteamVoiceChannel Channel;

	/** Channel speakers only: also play the player who owns this pawn when they transmit on the channel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam Voice", meta = (EditCondition = "Source == ESteamVoiceSpeakerSource::Channel", EditConditionHides, ToolTip = "Off: the walkie on A's pawn does not play A, so A is not heard twice next to their own mouth."))
	bool bIncludeOwner = false;

	/** Switches a Channel speaker to another channel (walkie dial). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (ToolTip = "Switches this Channel speaker to another channel (walkie dial)."))
	void SetChannel(FSteamVoiceChannel NewChannel);

	/** The player an Owner speaker plays right now (its owner once their voice is connected). Empty for a Channel speaker. */
	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (ToolTip = "The player an Owner speaker plays right now. Empty for a Channel speaker."))
	TArray<FSteamId> GetTalkers() const;

	/** Owner: true while its player talks. Channel: true while the channel has voice (until a second after the last word). */
	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (ToolTip = "Owner speaker: true while its player talks. Channel speaker: true while the channel has voice (until a second after the last word)."))
	bool IsAnyoneTalking() const;

	/** True while the speaker is switched on (Activate / Deactivate). Is Playing tells whether it makes sound right now. */
	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (ToolTip = "True while the speaker is switched on (Activate / Deactivate). Is Playing tells whether it makes sound right now."))
	bool IsSpeakerOn() const { return bSpeakerOn; }

	/** The pawn this speaker belongs to: the first pawn in its owner chain, or nullptr. */
	APawn* GetOwningPawn() const;

	/**
	 * Connects the component to decoded voice (game thread, called by the voice subsystem): a talker's audio (Owner) or a channel mix
	 * (Channel, InTalker invalid). Call before Start, or while playing to swap the source. InSampleRate is the rate the worker decodes at;
	 * it is fixed once the component has started playing. InBufferMs is the audio kept buffered before playing.
	 */
	void SetVoiceSource(FSteamId InTalker, Audio::FPatchOutputStrongPtr InOutput, int32 InSampleRate, int32 InBufferMs);

	/** Stops the sound and releases the voice it plays (game thread, called by the voice subsystem). */
	void ClearVoiceSource();

	/** True while connected to a talker or a channel mix. */
	bool HasVoiceSource() const { return bHasVoiceSource; }

	/** The talker this component plays. Invalid while it plays nobody. */
	FSteamId GetTalker() const { return Talker; }

	/** Buffered audio in milliseconds (any thread): the playout delay heard (largest fill of the last half second), 0 while not playing. */
	float GetBufferedMs() const;

	/** Times the buffer ran dry while playing (any thread). */
	uint32 GetUnderruns() const { return Underruns.load(std::memory_order_relaxed); }


	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Switches the speaker on. Does not start the sound: the plugin starts it when voice arrives. */
	virtual void Activate(bool bReset = false) override;
	/** Switches the speaker off: stops the sound and takes it out of the voice routing. */
	virtual void Deactivate() override;
	//~ End UActorComponent

#if WITH_EDITOR
	//~ Begin UObject
	/** Channel or Include Owner edited in the details panel while playing: routes the speaker again, like Set Channel. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject
#endif

protected:
	//~ Begin USynthComponent
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
	//~ End USynthComponent

private:
	/** Tells the voice subsystem the speaker was switched on or off (silences it at once when off). */
	void NotifySwitched(bool bWasOn);

	/** A Channel speaker's channel or owner rule changed: silences it and lets the routing connect it again on the next update. */
	void RerouteChannelSpeaker();

	/** Records the buffered samples (audio plus lead silence) at the start of a render block (render thread). */
	void UpdateFill(int32 Buffered, int32 NumSamples);

	/** Audio render thread state. Only touched there (set through SynthCommand). */
	struct FRenderState
	{
		Audio::FPatchOutputStrongPtr Output;
		int32 SampleRate = 48000;
		int32 TargetSamples = 0;
		int32 FadeSamples = 0;
		/** Silence still to play before the first audio, so it plays one playout delay after it arrived. */
		int32 LeadSilence = 0;
		/** Largest buffered samples in the current and the previous measuring window (the delay heard). */
		int32 FillMax = 0;
		int32 FillMaxPrevious = 0;
		int32 FillWindowSamples = 0;
		/** The next audio fades in (start after a gap). */
		bool bFadeIn = false;
		bool bPlaying = false;
	};

	FRenderState Render;
	FSteamId Talker;
	int32 VoiceSampleRate = 48000;

	/** On / off switch (Activate / Deactivate). Until one of them is called, Auto Activate decides at begin play. */
	bool bSpeakerOn = false;
	bool bSpeakerSwitched = false;
	bool bRegistered = false;
	bool bHasVoiceSource = false;

	std::atomic<int32> LastBufferedSamples{0};
	std::atomic<uint32> Underruns{0};
};
