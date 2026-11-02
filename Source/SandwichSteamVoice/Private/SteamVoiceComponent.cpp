// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceComponent.h"
#include "GameFramework/Pawn.h"
#include "SteamVoiceSubsystem.h"

namespace
{
	/** Fade length after a gap and before an underrun, so starts and stops do not click. */
	constexpr float FadeMs = 5.0f;

	/** The buffered amount is measured as its largest value over the last two windows of this length (right after a packet arrived). */
	constexpr float FillWindowSeconds = 0.25f;
}

USteamVoiceComponent::USteamVoiceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	// Auto Activate is the speaker's on / off state at begin play; the sound itself only starts when voice arrives.
	bAutoActivate = true;
}

void USteamVoiceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bSpeakerSwitched)
	{
		bSpeakerOn = bAutoActivate;
	}

	// Registered for the whole time it plays in the world, on or off: a switched off Owner speaker still claims its pawn (no speaker is added).
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
		{
			bRegistered = true;
			Voice->RegisterSpeaker(this);
		}
	}
}

void USteamVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegistered)
	{
		bRegistered = false;
		if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
		{
			Voice->UnregisterSpeaker(this);
		}
	}
	ClearVoiceSource();

	Super::EndPlay(EndPlayReason);
}

void USteamVoiceComponent::Activate(bool bReset)
{
	// Not the synth's Activate: that would start a silent sound. The plugin starts the sound on voice activity.
	const bool bWasOn = bSpeakerOn;
	bSpeakerSwitched = true;
	bSpeakerOn = true;
	NotifySwitched(bWasOn);

	if (!bWasOn || bReset)
	{
		OnComponentActivated.Broadcast(this, bReset);
	}
}

void USteamVoiceComponent::Deactivate()
{
	const bool bWasOn = bSpeakerOn;
	bSpeakerSwitched = true;
	bSpeakerOn = false;
	NotifySwitched(bWasOn);

	if (bWasOn)
	{
		OnComponentDeactivated.Broadcast(this);
	}
}

void USteamVoiceComponent::NotifySwitched(bool bWasOn)
{
	if (bWasOn == bSpeakerOn)
	{
		return;
	}

	if (!bSpeakerOn)
	{
		ClearVoiceSource(); // Silent at once; the routing drops it on the next update.
	}

	if (bRegistered)
	{
		if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
		{
			Voice->MarkSpeakersDirty();
		}
	}
}

void USteamVoiceComponent::SetChannel(FSteamVoiceChannel NewChannel)
{
	if (NewChannel == Channel)
	{
		return;
	}

	Channel = NewChannel;
	RerouteChannelSpeaker();
}

void USteamVoiceComponent::RerouteChannelSpeaker()
{
	if (bRegistered && Source == ESteamVoiceSpeakerSource::Channel)
	{
		// The routing picks the new channel up on the next update.
		ClearVoiceSource();
		if (USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(this))
		{
			Voice->MarkSpeakersDirty();
		}
	}
}

#if WITH_EDITOR
void USteamVoiceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// MemberProperty: an edit inside the Channel struct (its Id or Tag) reports the struct here.
	const FName Name = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (Name == GET_MEMBER_NAME_CHECKED(USteamVoiceComponent, Channel) || Name == GET_MEMBER_NAME_CHECKED(USteamVoiceComponent, bIncludeOwner))
	{
		RerouteChannelSpeaker();
	}
}
#endif

TArray<FSteamId> USteamVoiceComponent::GetTalkers() const
{
	const USteamVoiceSubsystem* Voice = bHasVoiceSource ? USteamVoiceSubsystem::Get(this) : nullptr;
	return Voice ? Voice->GetSpeakerTalkers(this) : TArray<FSteamId>();
}

bool USteamVoiceComponent::IsAnyoneTalking() const
{
	return !GetTalkers().IsEmpty();
}

float USteamVoiceComponent::GetVoiceLevel() const
{
	const USteamVoiceSubsystem* Voice = bHasVoiceSource ? USteamVoiceSubsystem::Get(this) : nullptr;
	if (!Voice)
	{
		return 0.0f;
	}

	float Level = 0.0f;
	for (const FSteamId& Player : Voice->GetSpeakerTalkers(this))
	{
		Level = FMath::Max(Level, Voice->GetPlayerVoiceLevel(Player));
	}
	return Level;
}

APawn* USteamVoiceComponent::GetOwningPawn() const
{
	for (AActor* Actor = GetOwner(); Actor; Actor = Actor->GetOwner())
	{
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn;
		}
	}
	return nullptr;
}

void USteamVoiceComponent::SetVoiceSource(FSteamId InTalker, Audio::FPatchOutputStrongPtr InOutput, int32 InSampleRate, int32 InBufferMs)
{
	check(IsInGameThread());

	Talker = InTalker;
	bHasVoiceSource = true;
	VoiceSampleRate = FMath::Clamp(InSampleRate, 11025, 48000);

	const int32 TargetSamples = FMath::Max(VoiceSampleRate * FMath::Clamp(InBufferMs, 0, 1000) / 1000, 1);
	const int32 FadeSamples = FMath::Max(FMath::RoundToInt(VoiceSampleRate * FadeMs / 1000.0f), 1);

	// The render state belongs to the audio render thread; the command runs there before the next render.
	SynthCommand([this, Output = MoveTemp(InOutput), SampleRate = VoiceSampleRate, TargetSamples, FadeSamples]() mutable
	{
		Render.Output = MoveTemp(Output);
		Render.SampleRate = SampleRate;
		Render.TargetSamples = TargetSamples;
		Render.FadeSamples = FadeSamples;
		Render.LeadSilence = 0;
		Render.bPlaying = false;
	});
}

void USteamVoiceComponent::ClearVoiceSource()
{
	check(IsInGameThread());

	if (!bHasVoiceSource)
	{
		return;
	}

	Talker = FSteamId();
	bHasVoiceSource = false;
	Stop();
	SynthCommand([this]()
	{
		Render.Output.Reset();
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
	int32 Buffered = Available + State.LeadSilence;

	// Far too much buffered (a burst after a stall upstream): drop the oldest audio back to the target instead of adding latency.
	const int32 MaxBuffered = State.TargetSamples * 3 + NumSamples;
	bool bResetFill = false;
	if (Buffered > MaxBuffered)
	{
		State.LeadSilence = 0;
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
		Buffered = Available;
		bResetFill = true;
	}

	if (!State.bPlaying)
	{
		if (Available <= 0)
		{
			FMemory::Memzero(OutAudio, sizeof(float) * NumSamples);
			LastBufferedSamples.store(0, std::memory_order_relaxed);
			return NumSamples;
		}

		// The first audio plays one playout delay after it arrived: silence first for the part not buffered yet. Waiting for a full
		// delay of audio instead overshoots by up to a packet (Steam sends 20 to 60 ms at once) on every start.
		State.bPlaying = true;
		State.bFadeIn = true;
		State.LeadSilence = FMath::Max(State.TargetSamples - Available, 0);
		Buffered = Available + State.LeadSilence;
		bResetFill = true;
	}

	if (bResetFill)
	{
		// The measurement starts from here: the old one belongs to the previous sentence (or to the audio just dropped).
		State.FillMax = Buffered;
		State.FillMaxPrevious = Buffered;
		State.FillWindowSamples = 0;
	}
	UpdateFill(Buffered, NumSamples);

	int32 Written = 0;
	if (State.LeadSilence > 0)
	{
		Written = FMath::Min(State.LeadSilence, NumSamples);
		FMemory::Memzero(OutAudio, sizeof(float) * Written);
		State.LeadSilence -= Written;
	}

	const int32 Wanted = NumSamples - Written;
	if (Wanted > 0)
	{
		float* Audio = OutAudio + Written;
		const int32 Popped = FMath::Clamp(State.Output->PopAudio(Audio, Wanted, false), 0, Wanted);

		if (State.bFadeIn && Popped > 0)
		{
			State.bFadeIn = false;
			const int32 FadeIn = FMath::Min(State.FadeSamples, Popped);
			for (int32 Index = 0; Index < FadeIn; ++Index)
			{
				Audio[Index] *= static_cast<float>(Index) / static_cast<float>(FadeIn);
			}
		}

		if (Popped < Wanted)
		{
			// Underrun: fade out what we have, then start again with the next audio.
			const int32 FadeOut = FMath::Min(State.FadeSamples, Popped);
			for (int32 Index = 0; Index < FadeOut; ++Index)
			{
				Audio[Popped - 1 - Index] *= static_cast<float>(Index) / static_cast<float>(FadeOut);
			}
			FMemory::Memzero(Audio + Popped, sizeof(float) * (Wanted - Popped));
			State.bPlaying = false;
			Underruns.fetch_add(1, std::memory_order_relaxed);
		}
	}

	LastBufferedSamples.store(State.bPlaying ? FMath::Max(State.FillMax, State.FillMaxPrevious) : 0, std::memory_order_relaxed);
	return NumSamples;
}

void USteamVoiceComponent::UpdateFill(int32 Buffered, int32 NumSamples)
{
	FRenderState& State = Render;

	// The delay heard is the fill right after a packet arrived (it then runs down until the next one): the largest fill over the
	// last two windows, so the packet sizes do not show as a jumping number.
	State.FillMax = FMath::Max(State.FillMax, Buffered);
	State.FillWindowSamples += NumSamples;
	if (State.FillWindowSamples >= FMath::RoundToInt(static_cast<float>(State.SampleRate) * FillWindowSeconds))
	{
		State.FillMaxPrevious = State.FillMax;
		State.FillMax = Buffered;
		State.FillWindowSamples = 0;
	}
}
