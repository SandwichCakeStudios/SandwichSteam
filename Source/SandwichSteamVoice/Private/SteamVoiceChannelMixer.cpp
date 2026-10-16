// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceChannelMixer.h"

namespace SandwichSteam::Voice
{
	namespace
	{
		/** Linear below the knee, then bends smoothly towards 1 (slope 1 at the knee, never reaches 1). Stateless, so it cannot pump. */
		FORCEINLINE float SoftLimit(float Sample, uint64& Limited)
		{
			constexpr float Knee = FChannelMixer::LimiterKnee;
			const float Magnitude = FMath::Abs(Sample);
			if (Magnitude <= Knee)
			{
				return Sample;
			}

			++Limited;
			const float Over = (Magnitude - Knee) / (1.0f - Knee);
			const float Limited01 = Knee + (1.0f - Knee) * (Over / (1.0f + Over));
			return Sample < 0.0f ? -Limited01 : Limited01;
		}
	}

	void FChannelMixer::Configure(int32 InSampleRate, double LeadSeconds)
	{
		SampleRate = FMath::Clamp(InSampleRate, 8000, 48000);
		Capacity = SampleRate; // One second.
		LeadSamples = FMath::Clamp(static_cast<int32>(LeadSeconds * SampleRate), 0, Capacity / 2);

		Outputs.Reset();
		Contributors.Reset();
		Scratch.SetNumUninitialized(Capacity);
		Flushed = 0;
		Origin = 0.0;
		LastAudio = -1.0;
		OverflowSamples = 0;
		bRunning = false;
	}

	void FChannelMixer::SetOutputs(TConstArrayView<uint64> Excluded)
	{
		TArray<FOutput> NewOutputs;
		NewOutputs.Reserve(Excluded.Num());
		for (const uint64 Id : Excluded)
		{
			if (NewOutputs.ContainsByPredicate([Id](const FOutput& Output) { return Output.Excluded == Id; }))
			{
				continue;
			}

			const int32 Existing = Outputs.IndexOfByPredicate([Id](const FOutput& Output) { return Output.Excluded == Id; });
			if (Existing != INDEX_NONE)
			{
				NewOutputs.Add(MoveTemp(Outputs[Existing]));
				continue;
			}

			FOutput& Added = NewOutputs.AddDefaulted_GetRef();
			Added.Excluded = Id;
			Added.Ring.SetNumZeroed(Capacity);
		}
		Outputs = MoveTemp(NewOutputs);
	}

	void FChannelMixer::Add(uint64 Talker, const float* Samples, int32 NumSamples, double NowSeconds)
	{
		if (NumSamples <= 0 || Outputs.IsEmpty())
		{
			return;
		}

		if (!bRunning)
		{
			// Continue the sample count where it stopped; the clock restarts now.
			bRunning = true;
			Origin = NowSeconds - static_cast<double>(Flushed) / SampleRate;
		}

		FContributor& Contributor = Contributors.FindOrAdd(Talker);
		if (Contributor.LastAudio < 0.0 || Contributor.Cursor < Flushed)
		{
			// Starting, or the clock overtook this talker (gap, late packets): start again one lead ahead.
			Contributor.Cursor = Flushed + LeadSamples;
		}

		const int64 Space = Flushed + Capacity - Contributor.Cursor;
		const int32 Written = static_cast<int32>(FMath::Clamp<int64>(Space, 0, NumSamples));
		OverflowSamples += static_cast<uint64>(NumSamples - Written);

		if (Written > 0)
		{
			for (FOutput& Output : Outputs)
			{
				if (Output.Excluded != Talker)
				{
					AddToRing(Output, Contributor.Cursor, Samples, Written);
					Output.LastAudio = NowSeconds;
				}
			}
			Contributor.Cursor += Written;
		}

		Contributor.LastAudio = NowSeconds;
		LastAudio = NowSeconds;
	}

	void FChannelMixer::AddToRing(FOutput& Output, int64 Start, const float* Samples, int32 NumSamples) const
	{
		const int32 First = static_cast<int32>(Start % Capacity);
		const int32 FirstCount = FMath::Min(NumSamples, Capacity - First);

		float* Ring = Output.Ring.GetData();
		for (int32 Index = 0; Index < FirstCount; ++Index)
		{
			Ring[First + Index] += Samples[Index];
		}
		for (int32 Index = FirstCount; Index < NumSamples; ++Index)
		{
			Ring[Index - FirstCount] += Samples[Index];
		}
	}

	void FChannelMixer::Advance(double NowSeconds, TFunctionRef<void(int32, TConstArrayView<float>)> Emit)
	{
		if (!bRunning)
		{
			return;
		}

		const int64 Due = static_cast<int64>(FMath::FloorToDouble((NowSeconds - Origin) * SampleRate));
		int32 Count = static_cast<int32>(FMath::Min<int64>(Due - Flushed, Capacity));
		if (Count <= 0)
		{
			return;
		}
		if (Due - Flushed > Capacity)
		{
			// The worker stalled for more than the ring holds: emit one ring and move the clock, instead of catching up forever.
			Origin = NowSeconds - static_cast<double>(Flushed + Count) / SampleRate;
		}

		const int32 First = static_cast<int32>(Flushed % Capacity);
		const int32 FirstCount = FMath::Min(Count, Capacity - First);

		for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
		{
			FOutput& Output = Outputs[OutputIndex];
			float* Ring = Output.Ring.GetData();
			float* Out = Scratch.GetData();

			FMemory::Memcpy(Out, Ring + First, sizeof(float) * FirstCount);
			FMemory::Memzero(Ring + First, sizeof(float) * FirstCount);
			if (Count > FirstCount)
			{
				FMemory::Memcpy(Out + FirstCount, Ring, sizeof(float) * (Count - FirstCount));
				FMemory::Memzero(Ring, sizeof(float) * (Count - FirstCount));
			}

			// Headroom per extra talker, ramped over the block so a talker joining or leaving does not click.
			const int32 Active = CountActiveTalkers(Output.Excluded, NowSeconds);
			const float Target = Active <= 1 ? 1.0f : FMath::Pow(10.0f, HeadroomDbPerTalker * static_cast<float>(Active - 1) / 20.0f);
			const float Step = (Target - Output.Gain) / static_cast<float>(Count);
			float Gain = Output.Gain;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Gain += Step;
				Out[Index] = SoftLimit(Out[Index] * Gain, Output.Limited);
			}
			Output.Gain = Target;

			Emit(OutputIndex, TConstArrayView<float>(Out, Count));
		}
		Flushed += Count;

		if (LastAudio >= 0.0 && NowSeconds - LastAudio > IdleSeconds)
		{
			bool bPending = false;
			for (const TPair<uint64, FContributor>& Pair : Contributors)
			{
				bPending |= Pair.Value.Cursor > Flushed;
			}
			if (!bPending)
			{
				bRunning = false;
				Contributors.Reset(); // Everyone starts with a fresh lead next time.
			}
		}
	}

	void FChannelMixer::RemoveTalker(uint64 Talker)
	{
		Contributors.Remove(Talker);
	}

	int32 FChannelMixer::CountActiveTalkers(uint64 Excluded, double NowSeconds) const
	{
		int32 Active = 0;
		for (const TPair<uint64, FContributor>& Pair : Contributors)
		{
			if (Pair.Key != Excluded && Pair.Value.LastAudio >= 0.0 && NowSeconds - Pair.Value.LastAudio <= ActiveTalkerSeconds)
			{
				++Active;
			}
		}
		return Active;
	}

	FChannelMixer::FOutputStats FChannelMixer::GetOutputStats(int32 Output, double NowSeconds) const
	{
		const FOutput& Source = Outputs[Output];
		FOutputStats Stats;
		Stats.Excluded = Source.Excluded;
		Stats.ActiveTalkers = CountActiveTalkers(Source.Excluded, NowSeconds);
		Stats.Gain = Source.Gain;
		Stats.Limited = Source.Limited;
		return Stats;
	}
}
