// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "UObject/WeakObjectPtr.h"

#include <atomic>

/**
 * Hands work from Steam's callback thread over to the game thread.
 *
 * Steam callbacks (STEAM_CALLBACK / CCallResult) run on whichever thread pumps SteamAPI_RunCallbacks.
 * The Steam Online Subsystem pumps them on its online thread, so a raw callback body must do nothing
 * except copy its payload and Enqueue(). All UObject access, cache changes and delegate broadcasts then
 * happen in the queued work on the game thread.
 *
 * Idle cost is zero: the ticker is registered by the first Enqueue() and removed again once the queue is empty.
 * Owned by USteamCoreSubsystem. Always create with MakeShared<FSteamCallbackDispatcher, ESPMode::ThreadSafe>().
 */
class SANDWICHSTEAM_API FSteamCallbackDispatcher : public TSharedFromThis<FSteamCallbackDispatcher, ESPMode::ThreadSafe>
{
public:
	using FWork = TUniqueFunction<void()>;

	struct FStats
	{
		uint64 Enqueued = 0;
		uint64 Drained = 0;
		uint64 Dropped = 0;
		uint64 TickerRegistrations = 0;
		int32 Pending = 0;
	};

	FSteamCallbackDispatcher() = default;
	FSteamCallbackDispatcher(const FSteamCallbackDispatcher&) = delete;
	FSteamCallbackDispatcher& operator=(const FSteamCallbackDispatcher&) = delete;

	/** Queue work for the game thread. Callable from any thread. Ignored after Shutdown(). */
	void Enqueue(FWork Work);

	/**
	 * Queue work that only runs if Owner is still alive when the game thread drains the queue.
	 * Func receives a reference to the owner. Skipped work is counted as dropped.
	 */
	template <typename T, typename FuncType>
	void EnqueueFor(TWeakObjectPtr<T> Owner, FuncType&& Func)
	{
		Enqueue([this, Owner, Function = Forward<FuncType>(Func)]() mutable
		{
			if (T* StrongOwner = Owner.Get())
			{
				Function(*StrongOwner);
			}
			else
			{
				DroppedCount.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}

	/**
	 * Run everything that was queued when the call started, in order. Game thread only.
	 * The ticker calls this; automation tests call it directly. Returns the number of items run.
	 */
	int32 Drain();

	/** Discard pending work and ignore further Enqueue() calls. Game thread only. */
	void Shutdown();

	FStats GetStats() const;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Thread that made the most recent Enqueue() call. For a raw Steam callback this is the callback thread. */
	uint32 GetLastEnqueueThreadId() const { return LastEnqueueThreadId.load(std::memory_order_relaxed); }

	/** True when the most recent Enqueue() call was made on the game thread. */
	bool WasLastEnqueueOnGameThread() const { return bLastEnqueueOnGameThread.load(std::memory_order_relaxed); }
#endif

private:
	void EnsureTickerScheduled();
	bool Tick();

	TQueue<FWork, EQueueMode::Mpsc> Queue;
	std::atomic<int32> PendingCount{0};
	std::atomic<bool> bTickerScheduled{false};
	std::atomic<bool> bClosed{false};

	std::atomic<uint64> EnqueuedCount{0};
	std::atomic<uint64> DrainedCount{0};
	std::atomic<uint64> DroppedCount{0};
	std::atomic<uint64> TickerRegistrationCount{0};

#if SANDWICHSTEAM_WITH_DEBUG
	std::atomic<uint32> LastEnqueueThreadId{0};
	std::atomic<bool> bLastEnqueueOnGameThread{false};
#endif
};

/**
 * Queue work on a dispatcher (pointer, TSharedPtr or TSharedRef) that runs only while WeakOwner is alive.
 * Usage: SANDWICHSTEAM_DISPATCH(Dispatcher, WeakSubsystem, [Value](USteamFooSubsystem& Foo) { Foo.HandleValue(Value); });
 */
#define SANDWICHSTEAM_DISPATCH(Dispatcher, WeakOwner, ...) (Dispatcher)->EnqueueFor((WeakOwner), __VA_ARGS__)
