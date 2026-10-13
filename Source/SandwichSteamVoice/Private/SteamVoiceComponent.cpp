// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceComponent.h"

namespace
{
	/** Fade length after a gap and before an underrun, so starts and stops do not click. */
	constexpr float FadeMs = 5.0f;
}

USteamVoiceComponent::USteamVoiceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = false;
}

void USteamVoiceComponent::SetVoiceSource(FSteamId InTalker, Audio::FPatchOutputStrongPtr InOutput, int32 InSampleRate, int32 InPlayoutDelayMs)
{
	check(IsInGameThread());

	Talker = InTalker;
	VoiceSampleRate = FMath::Clamp(InSampleRate, 8000, 48000);

	const int32 TargetSamples = FMath::Max(VoiceSampleRate * FMath::Clamp(InPlayoutDelayMs, 0, 1000) / 1000, 1);
	const int32 FadeSamples = FMath::Max(FMath::RoundToInt(VoiceSampleRate * FadeMs / 1000.0f), 1);

	// The render state belongs to the audio render thread; the command runs there before the next render.
	SynthCommand([this, Output = MoveTemp(InOutput), TargetSamples, FadeSamples]() mutable
	{
		Render.Output = MoveTemp(Output);
		Render.TargetSamples = TargetSamples;
		Render.FadeSamples = FadeSamples;
		Render.bPlaying = false;
	});
}

float USteamVoiceComponent::GetBufferedMs() const
{
	return VoiceSampleRate > 0 ? 1000.0f * static_cast<float>(LastBufferedSamples.load(std::memory_order_relaxed)) / static_cast<float>(VoiceSampleRate) : 0.0f;
}

bool USteamVoiceComponent::Init(int32& SampleRate)
{
	// Mono at the rate the worker decodes; the mixer resamples to the device rate.
	NumChannels = 1;
	SampleRate = VoiceSampleRate;
	return true;
}

int32 USteamVoiceComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FRenderState& State = Render;
	if (!State.Output.IsValid() || NumSamples <= 0)
	{
		FMemory::Memzero(OutAudio, sizeof(float) * FMath::Max(NumSamples, 0));
		LastBufferedSamples.store(0, std::memory_order_relaxed);
		return NumSamples;
	}

	int32 Available = State.Output->GetNumSamplesAvailable();

	// Far too much buffered (a burst after a stall upstream): drop the oldest audio back to the target instead of adding latency.
	const int32 MaxBuffered = State.TargetSamples * 3 + NumSamples;
	if (Available > MaxBuffered)
	{
		int32 Excess = Available - State.TargetSamples;
		while (Excess > 0)
		{
			const int32 Dropped = State.Output->PopAudio(OutAudio, FMath::Min(Excess, NumSamples), false);
			if (Dropped <= 0)
			{
				break;
			}
			Excess -= Dropped;
			Available -= Dropped;
		}
	}

	bool bStarting = false;
	if (!State.bPlaying)
	{
		if (Available < State.TargetSamples)
		{
			FMemory::Memzero(OutAudio, sizeof(float) * NumSamples);
			LastBufferedSamples.store(Available, std::memory_order_relaxed);
			return NumSamples;
		}
		State.bPlaying = true;
		bStarting = true;
	}

	const int32 Popped = FMath::Clamp(State.Output->PopAudio(OutAudio, NumSamples, false), 0, NumSamples);

	if (bStarting)
	{
		const int32 FadeIn = FMath::Min(State.FadeSamples, Popped);
		for (int32 Index = 0; Index < FadeIn; ++Index)
		{
			OutAudio[Index] *= static_cast<float>(Index) / static_cast<float>(FadeIn);
		}
	}

	if (Popped < NumSamples)
	{
		// Underrun: fade out what we have, then buffer up to the target again.
		const int32 FadeOut = FMath::Min(State.FadeSamples, Popped);
		for (int32 Index = 0; Index < FadeOut; ++Index)
		{
			OutAudio[Popped - 1 - Index] *= static_cast<float>(Index) / static_cast<float>(FadeOut);
		}
		FMemory::Memzero(OutAudio + Popped, sizeof(float) * (NumSamples - Popped));
		State.bPlaying = false;
		Underruns.fetch_add(1, std::memory_order_relaxed);
	}

	LastBufferedSamples.store(FMath::Max(Available - Popped, 0), std::memory_order_relaxed);
	return NumSamples;
}
