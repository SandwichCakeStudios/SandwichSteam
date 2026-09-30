// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Decides when changed stats are uploaded with StoreStats. Pure logic (no Steam, no UObject, no clock of its own),
 * so it is unit tested.
 *
 *  - Changes are coalesced: the first change starts a window and one store happens when the window has passed,
 *    no matter how many changes arrive in between.
 *  - An urgent change (achievement unlocked) stores at once.
 *  - Only one store is in flight at a time. Changes made while one is in flight are stored after it completed.
 *  - A failed store keeps the data dirty so it is retried after the next window.
 */
class FSteamStatsFlushPolicy
{
public:
	explicit FSteamStatsFlushPolicy(double InIntervalSeconds = 5.0)
		: IntervalSeconds(FMath::Max(InIntervalSeconds, 0.0))
	{
	}

	void SetInterval(double InIntervalSeconds) { IntervalSeconds = FMath::Max(InIntervalSeconds, 0.0); }
	double GetInterval() const { return IntervalSeconds; }

	/** Something changed. Now is a monotonic time in seconds. */
	void MarkDirty(double Now)
	{
		if (!bDirty)
		{
			bDirty = true;
			DirtySince = Now;
		}
	}

	/** Something changed that must be stored immediately. */
	void MarkUrgent(double Now)
	{
		MarkDirty(Now);
		bUrgent = true;
	}

	bool IsDirty() const { return bDirty; }
	bool IsStoreInFlight() const { return bInFlight; }

	/** True when a store should start now. */
	bool ShouldFlush(double Now) const
	{
		if (!bDirty || bInFlight)
		{
			return false;
		}
		return bUrgent || (Now - DirtySince) >= IntervalSeconds;
	}

	/** A store was started. Changes made from now on belong to the next store. */
	void OnFlushStarted()
	{
		bDirty = false;
		bUrgent = false;
		bInFlight = true;
	}

	/** The store finished. On failure the data stays dirty and is retried after the next window. */
	void OnFlushCompleted(bool bSuccess, double Now)
	{
		bInFlight = false;
		if (!bSuccess)
		{
			MarkDirty(Now);
		}
	}

	/** Forget everything (feature shut down). */
	void Reset()
	{
		bDirty = false;
		bUrgent = false;
		bInFlight = false;
		DirtySince = 0.0;
	}

private:
	double IntervalSeconds = 5.0;
	double DirtySince = 0.0;
	bool bDirty = false;
	bool bUrgent = false;
	bool bInFlight = false;
};
