// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Voice activation for the local microphone (pure, time is passed in, so it can be unit tested). */
namespace SandwichSteam::Voice
{
	/**
	 * Decides per captured packet whether it is sent, from its level (RMS, 0 to 1). A packet at or above the threshold opens the gate;
	 * it stays open for the hold time so word ends are kept. The last quiet packet before the gate opens is sent first (pre-roll),
	 * so word starts are kept too. Threshold 0 sends everything.
	 */
	class FActivationGate
	{
	public:
		enum class EDecision : uint8
		{
			/** Send the packet. */
			Send,
			/** Send the kept pre-roll packet first, then this one. */
			SendWithPreRoll,
			/** Do not send; keep the packet as the pre-roll candidate. */
			Hold
		};

		void Configure(double InHoldSeconds);
		void SetThreshold(float InThreshold) { Threshold = FMath::Clamp(InThreshold, 0.0f, 1.0f); }
		float GetThreshold() const { return Threshold; }

		/** Level below 0 means unknown (the packet could not be measured): it is sent. */
		EDecision Evaluate(float Level, double NowSeconds);

		/** Packets held back in total. */
		uint64 GetHeld() const { return Held; }

		/** Closes the gate and forgets the pre-roll (the microphone opened again). Keeps the threshold. */
		void Reset();

		/** A pre-roll packet older than this belongs to an earlier phrase and is not sent. */
		static constexpr double PreRollMaxAgeSeconds = 0.15;

	private:
		float Threshold = 0.0f;
		double HoldSeconds = 0.3;
		double OpenUntil = -1.0;
		double PreRollTime = -1.0;
		bool bHasPreRoll = false;
		uint64 Held = 0;
	};
}
