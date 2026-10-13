// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Core/SteamId.h"
#include "DSP/MultithreadedPatching.h"

#include <atomic>

#include "SteamVoiceComponent.generated.h"

/**
 * Plays Steam voice through a normal Unreal audio component. Attenuation, spatialization, sound class, submix sends,
 * source effects and concurrency are the component's usual audio settings: the plugin only delivers clean voice audio.
 *
 * The voice worker decodes each talker and pushes the audio into a patch; this component reads it on the audio render thread
 * and keeps PlayoutDelayMs of audio buffered (fades in after a gap, fades out on an underrun).
 */
UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent, DisplayName = "Steam Voice"))
class SANDWICHSTEAMVOICE_API USteamVoiceComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	USteamVoiceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Connects the component to a talker's decoded audio (game thread). Call before Start, or while playing to swap the source.
	 * InSampleRate is the rate the worker decodes at; it is fixed once the component has started playing.
	 */
	void SetVoiceSource(FSteamId InTalker, Audio::FPatchOutputStrongPtr InOutput, int32 InSampleRate, int32 InPlayoutDelayMs);

	/** The talker this component plays. */
	FSteamId GetTalker() const { return Talker; }

	/** Buffered audio in milliseconds at the last audio render (any thread). */
	float GetBufferedMs() const;

	/** Times the buffer ran dry while playing (any thread). */
	uint32 GetUnderruns() const { return Underruns.load(std::memory_order_relaxed); }

protected:
	//~ Begin USynthComponent
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
	//~ End USynthComponent

private:
	/** Audio render thread state. Only touched there (set through SynthCommand). */
	struct FRenderState
	{
		Audio::FPatchOutputStrongPtr Output;
		int32 TargetSamples = 0;
		int32 FadeSamples = 0;
		bool bPlaying = false;
	};

	FRenderState Render;
	FSteamId Talker;
	int32 VoiceSampleRate = 48000;

	std::atomic<int32> LastBufferedSamples{0};
	std::atomic<uint32> Underruns{0};
};
