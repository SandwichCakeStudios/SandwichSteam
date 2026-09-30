// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamCallbackDispatcher.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformTLS.h"

void FSteamCallbackDispatcher::Enqueue(FWork Work)
{
	if (bClosed.load(std::memory_order_acquire))
	{
		DroppedCount.fetch_add(1, std::memory_order_relaxed);
		return;
	}

#if SANDWICHSTEAM_WITH_DEBUG
	LastEnqueueThreadId.store(FPlatformTLS::GetCurrentThreadId(), std::memory_order_relaxed);
	bLastEnqueueOnGameThread.store(IsInGameThread(), std::memory_order_relaxed);
#endif

	Queue.Enqueue(MoveTemp(Work));
	PendingCount.fetch_add(1, std::memory_order_acq_rel);
	EnqueuedCount.fetch_add(1, std::memory_order_relaxed);

	EnsureTickerScheduled();
}

void FSteamCallbackDispatcher::EnsureTickerScheduled()
{
	// Only the caller that flips the flag registers the ticker. FTSTicker is safe to use from any thread.
	if (bTickerScheduled.exchange(true, std::memory_order_acq_rel))
	{
		return;
	}

	TickerRegistrationCount.fetch_add(1, std::memory_order_relaxed);

	const TWeakPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> WeakThis = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
	{
		if (const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Self = WeakThis.Pin())
		{
			return Self->Tick();
		}
		return false;
	}));
}

bool FSteamCallbackDispatcher::Tick()
{
	Drain();

	// Release the flag, then re-check: an Enqueue() that raced with the release either registered its own
	// ticker (flag was false) or left work behind that this ticker keeps alive for another frame.
	bTickerScheduled.store(false, std::memory_order_release);
	if (!Queue.IsEmpty() && !bTickerScheduled.exchange(true, std::memory_order_acq_rel))
	{
		return true;
	}
	return false;
}

int32 FSteamCallbackDispatcher::Drain()
{
	check(IsInGameThread());

	// Work queued by the work itself waits for the next drain, so a self-requeueing item cannot starve the frame.
	const int32 Limit = PendingCount.load(std::memory_order_acquire);
	int32 Processed = 0;

	while (Processed < Limit)
	{
		FWork Work;
		if (!Queue.Dequeue(Work))
		{
			break;
		}

		++Processed;
		PendingCount.fetch_sub(1, std::memory_order_acq_rel);
		DrainedCount.fetch_add(1, std::memory_order_relaxed);

		if (Work)
		{
			Work();
		}
	}

	return Processed;
}

void FSteamCallbackDispatcher::Shutdown()
{
	check(IsInGameThread());

	bClosed.store(true, std::memory_order_release);

	FWork Discarded;
	while (Queue.Dequeue(Discarded))
	{
		PendingCount.fetch_sub(1, std::memory_order_acq_rel);
		DroppedCount.fetch_add(1, std::memory_order_relaxed);
	}
}

FSteamCallbackDispatcher::FStats FSteamCallbackDispatcher::GetStats() const
{
	FStats Stats;
	Stats.Enqueued = EnqueuedCount.load(std::memory_order_relaxed);
	Stats.Drained = DrainedCount.load(std::memory_order_relaxed);
	Stats.Dropped = DroppedCount.load(std::memory_order_relaxed);
	Stats.TickerRegistrations = TickerRegistrationCount.load(std::memory_order_relaxed);
	Stats.Pending = PendingCount.load(std::memory_order_relaxed);
	return Stats;
}
