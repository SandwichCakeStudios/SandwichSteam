// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Async/Async.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamIdLibrary.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamDispatcherOrderTest, "SandwichSteam.Core.Dispatcher.DrainsInOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamDispatcherOrderTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = MakeShared<FSteamCallbackDispatcher, ESPMode::ThreadSafe>();

	TArray<int32> Order;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Dispatcher->Enqueue([&Order, Index]() { Order.Add(Index); });
	}

	TestEqual(TEXT("Nothing runs before Drain"), Order.Num(), 0);
	TestEqual(TEXT("Drain reports the work it ran"), Dispatcher->Drain(), 5);
	TestEqual(TEXT("All work ran"), Order.Num(), 5);

	bool bInOrder = true;
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		bInOrder &= (Order[Index] == Index);
	}
	TestTrue(TEXT("Work ran in enqueue order"), bInOrder);

	TestEqual(TEXT("Second Drain is empty"), Dispatcher->Drain(), 0);
	TestEqual(TEXT("Stats: enqueued"), static_cast<int32>(Dispatcher->GetStats().Enqueued), 5);
	TestEqual(TEXT("Stats: drained"), static_cast<int32>(Dispatcher->GetStats().Drained), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamDispatcherDeadOwnerTest, "SandwichSteam.Core.Dispatcher.SkipsDeadOwners",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamDispatcherDeadOwnerTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = MakeShared<FSteamCallbackDispatcher, ESPMode::ThreadSafe>();

	UObject* LiveOwner = NewObject<USteamIdLibrary>(GetTransientPackage());
	UObject* DeadOwner = NewObject<USteamIdLibrary>(GetTransientPackage());
	const TWeakObjectPtr<UObject> WeakLive = LiveOwner;
	const TWeakObjectPtr<UObject> WeakDead = DeadOwner;

	int32 LiveRuns = 0;
	int32 DeadRuns = 0;
	Dispatcher->EnqueueFor(WeakLive, [&LiveRuns](UObject&) { ++LiveRuns; });
	SANDWICHSTEAM_DISPATCH(Dispatcher, WeakDead, [&DeadRuns](UObject&) { ++DeadRuns; });

	// The owner dies between the raw callback and the drain.
	DeadOwner->MarkAsGarbage();

	Dispatcher->Drain();

	TestEqual(TEXT("Live owner work ran"), LiveRuns, 1);
	TestEqual(TEXT("Dead owner work was skipped"), DeadRuns, 0);
	TestEqual(TEXT("Skipped work is counted as dropped"), static_cast<int32>(Dispatcher->GetStats().Dropped), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamDispatcherThreadsTest, "SandwichSteam.Core.Dispatcher.EnqueueFromOtherThreads",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamDispatcherThreadsTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = MakeShared<FSteamCallbackDispatcher, ESPMode::ThreadSafe>();

	constexpr int32 TaskCount = 8;
	constexpr int32 ItemsPerTask = 100;
	int32 Runs = 0;
	bool bAllOnGameThread = true;

	TArray<TFuture<void>> Futures;
	for (int32 Task = 0; Task < TaskCount; ++Task)
	{
		Futures.Add(Async(EAsyncExecution::ThreadPool, [Dispatcher, &Runs, &bAllOnGameThread]()
		{
			for (int32 Item = 0; Item < ItemsPerTask; ++Item)
			{
				Dispatcher->Enqueue([&Runs, &bAllOnGameThread]()
				{
					bAllOnGameThread &= IsInGameThread();
					++Runs;
				});
			}
		}));
	}

	for (TFuture<void>& Future : Futures)
	{
		Future.Wait();
	}

	// Drain may need several passes if the ticker did not run; each pass handles what was queued when it started.
	while (Dispatcher->Drain() > 0)
	{
	}

	TestEqual(TEXT("Every item from every thread ran"), Runs, TaskCount * ItemsPerTask);
	TestTrue(TEXT("Work ran on the game thread"), bAllOnGameThread);
	TestEqual(TEXT("Nothing pending"), Dispatcher->GetStats().Pending, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
