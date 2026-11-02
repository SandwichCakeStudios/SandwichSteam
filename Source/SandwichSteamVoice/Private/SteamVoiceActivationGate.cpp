// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceActivationGate.h"

namespace SandwichSteam::Voice
{
	void FActivationGate::Configure(double InHoldSeconds)
	{
		HoldSeconds = FMath::Max(InHoldSeconds, 0.0);
		Reset();
	}

	void FActivationGate::Reset()
	{
		OpenUntil = -1.0;
		PreRollTime = -1.0;
		bHasPreRoll = false;
	}

	FActivationGate::EDecision FActivationGate::Evaluate(float Level, double NowSeconds)
	{
		if (Threshold <= 0.0f || Level < 0.0f)
		{
			bHasPreRoll = false;
			return EDecision::Send;
		}

		if (Level >= Threshold)
		{
			OpenUntil = NowSeconds + HoldSeconds;
			const bool bPreRoll = bHasPreRoll && NowSeconds - PreRollTime <= PreRollMaxAgeSeconds;
			bHasPreRoll = false;
			return bPreRoll ? EDecision::SendWithPreRoll : EDecision::Send;
		}

		if (NowSeconds <= OpenUntil)
		{
			return EDecision::Send;
		}

		bHasPreRoll = true;
		PreRollTime = NowSeconds;
		++Held;
		return EDecision::Hold;
	}
}
