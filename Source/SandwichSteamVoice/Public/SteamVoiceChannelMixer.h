// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/** Mixes everyone transmitting on one voice channel (pure, time is passed in, so it can be unit tested). */
namespace SandwichSteam::Voice
{
	/**
	 * One channel's mix on the worker's clock. Talkers add decoded blocks whenever they arrive; Advance emits the mix at the real-time rate,
	 * so several talkers line up even though their packets arrive at different times.
	 *
	 * A talker who starts (or comes back after a gap) is placed Lead samples ahead of the clock: the lead absorbs network jitter, as the
	 * playout delay does for a mouth speaker. While the channel runs, gaps between words are emitted as silence (the speakers' buffers stay
	 * steady); it stops after IdleSeconds without audio.
	 *
	 * Outputs: one per excluded player. Excluded 0 is the full mix; any other id is "mix minus that player" (a walkie on A's pawn does not
	 * play A). Each output has its own headroom (-3 dB per extra active talker, ramped) and a soft limiter (no pumping).
	 *
	 * Steady use does not allocate: rings and scratch are sized once (one second).
	 */
	class FChannelMixer
	{
	public:
		/** Audio within this time counts a talker as active for the headroom. */
		static constexpr double ActiveTalkerSeconds = 0.3;
		/** No audio added for this long: the channel stops emitting. */
		static constexpr double IdleSeconds = 1.0;
		static constexpr float HeadroomDbPerTalker = -3.0f;
		/** The soft limiter is linear below this level and bends towards 1 above it. */
		static constexpr float LimiterKnee = 0.8f;

		struct FOutputStats
		{
			uint64 Excluded = 0;
			int32 ActiveTalkers = 0;
			float Gain = 1.0f;
			/** Samples bent by the limiter. */
			uint64 Limited = 0;
		};

		/** Sets the rate and lead and forgets everything (outputs included). */
		void Configure(int32 InSampleRate, double LeadSeconds);

		/**
		 * The wanted outputs, by excluded player (0 = full mix). Outputs that stay keep their audio; new ones start empty.
		 * Output indices (Advance, GetOutputLastAudio) follow this order.
		 */
		void SetOutputs(TConstArrayView<uint64> Excluded);

		int32 NumOutputs() const { return Outputs.Num(); }
		uint64 GetOutputExcluded(int32 Output) const { return Outputs[Output].Excluded; }

		/** Adds a talker's decoded block (mono, -1 to 1). Audio beyond one second ahead of the clock is dropped and counted. */
		void Add(uint64 Talker, const float* Samples, int32 NumSamples, double NowSeconds);

		/** Emits the mix up to NowSeconds: Emit(OutputIndex, Samples) per output, every output the same length. */
		void Advance(double NowSeconds, TFunctionRef<void(int32, TConstArrayView<float>)> Emit);

		/** Forgets a talker (left the game). Audio already added stays in the mix. */
		void RemoveTalker(uint64 Talker);

		/** True while the channel emits (audio within IdleSeconds). */
		bool IsRunning() const { return bRunning; }

		/** When audio last went into an output, -1 when never. A mix minus a player gets nothing while only that player talks. */
		double GetOutputLastAudio(int32 Output) const { return Outputs[Output].LastAudio; }

		FOutputStats GetOutputStats(int32 Output, double NowSeconds) const;

		/** Samples dropped because a talker was more than one second ahead of the clock. */
		uint64 GetOverflowSamples() const { return OverflowSamples; }

	private:
		struct FContributor
		{
			/** Absolute sample index where this talker's next sample goes. */
			int64 Cursor = 0;
			double LastAudio = -1.0;
		};

		struct FOutput
		{
			uint64 Excluded = 0;
			TArray<float> Ring;
			float Gain = 1.0f;
			double LastAudio = -1.0;
			uint64 Limited = 0;
		};

		int32 CountActiveTalkers(uint64 Excluded, double NowSeconds) const;
		void AddToRing(FOutput& Output, int64 Start, const float* Samples, int32 NumSamples) const;

		TArray<FOutput> Outputs;
		TMap<uint64, FContributor> Contributors;
		TArray<float> Scratch;
		int32 SampleRate = 48000;
		int32 Capacity = 48000;
		int32 LeadSamples = 0;
		/** Samples emitted so far (absolute index of the next one). */
		int64 Flushed = 0;
		/** Clock origin: Flushed samples were due at Origin + Flushed / SampleRate. */
		double Origin = 0.0;
		double LastAudio = -1.0;
		uint64 OverflowSamples = 0;
		bool bRunning = false;
	};
}
