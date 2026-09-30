// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Decides when the Steam progress toast for an achievement is shown. Pure logic, unit tested.
 *
 * Progress is split into steps of StepPercent. The toast is allowed when progress reaches a higher step than the last
 * one shown. Never at or above Max (Steam unlocks the achievement itself then), never for 0 progress. Progress that
 * goes back down (stat reset) lowers the remembered step silently.
 */
class FSteamProgressThrottle
{
public:
	explicit FSteamProgressThrottle(int32 InStepPercent = 10)
		: StepPercent(FMath::Clamp(InStepPercent, 1, 100))
	{
	}

	void SetStepPercent(int32 InStepPercent) { StepPercent = FMath::Clamp(InStepPercent, 1, 100); }

	/** True when the toast should be shown now. Remembers the step when it returns true. */
	bool ShouldIndicate(FName Key, int32 Current, int32 Max)
	{
		if (Max <= 0 || Current <= 0 || Current >= Max)
		{
			return false;
		}

		const int32 Step = static_cast<int32>((static_cast<int64>(Current) * 100 / Max) / StepPercent);
		int32& LastStep = LastSteps.FindOrAdd(Key, -1);

		if (Step > LastStep)
		{
			LastStep = Step;
			return true;
		}

		LastStep = Step; // Progress went back down, so the next rise shows the toast again.
		return false;
	}

	/** Forget one achievement (unlocked or cleared). */
	void Forget(FName Key) { LastSteps.Remove(Key); }

	void Reset() { LastSteps.Reset(); }

private:
	int32 StepPercent = 10;
	TMap<FName, int32> LastSteps;
};
