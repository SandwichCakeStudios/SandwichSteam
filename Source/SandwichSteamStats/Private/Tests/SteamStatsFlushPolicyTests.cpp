// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamStatsFlushPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamStatsFlushCoalescesTest, "SandwichSteam.Stats.FlushPolicy.CoalescesChanges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamStatsFlushCoalescesTest::RunTest(const FString& Parameters)
{
	FSteamStatsFlushPolicy Policy(5.0);

	// 100 changes in one second (10 ms apart): nothing is stored inside the window.
	int32 Stores = 0;
	double Now = 0.0;
	for (int32 Change = 0; Change < 100; ++Change)
	{
		Policy.MarkDirty(Now);
		if (Policy.ShouldFlush(Now))
		{
			Policy.OnFlushStarted();
			Policy.OnFlushCompleted(true, Now);
			++Stores;
		}
		Now += 0.01;
	}
	TestEqual(TEXT("No store inside the first second"), Stores, 0);

	// The window is measured from the first change, so the store happens once it has passed.
	TestFalse(TEXT("Not due at 4.9 s"), Policy.ShouldFlush(4.9));
	TestTrue(TEXT("Due at 5.0 s"), Policy.ShouldFlush(5.0));

	Policy.OnFlushStarted();
	Policy.OnFlushCompleted(true, 5.0);
	TestFalse(TEXT("Clean after a successful store"), Policy.IsDirty());
	TestFalse(TEXT("Nothing to store when clean"), Policy.ShouldFlush(100.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamStatsFlushUrgentTest, "SandwichSteam.Stats.FlushPolicy.UrgentStoresImmediately",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamStatsFlushUrgentTest::RunTest(const FString& Parameters)
{
	FSteamStatsFlushPolicy Policy(5.0);

	TestFalse(TEXT("Nothing to store before any change"), Policy.ShouldFlush(0.0));

	Policy.MarkUrgent(1.0);
	TestTrue(TEXT("An urgent change stores at once"), Policy.ShouldFlush(1.0));

	Policy.OnFlushStarted();
	TestFalse(TEXT("Cleared once the store started"), Policy.IsDirty());
	TestTrue(TEXT("One store is in flight"), Policy.IsStoreInFlight());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamStatsFlushInFlightTest, "SandwichSteam.Stats.FlushPolicy.OneStoreInFlight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamStatsFlushInFlightTest::RunTest(const FString& Parameters)
{
	FSteamStatsFlushPolicy Policy(5.0);

	Policy.MarkUrgent(0.0);
	Policy.OnFlushStarted();

	// Changes made while the store is in flight wait for it.
	Policy.MarkUrgent(0.5);
	TestFalse(TEXT("No second store while one is in flight"), Policy.ShouldFlush(0.5));

	Policy.OnFlushCompleted(true, 1.0);
	TestTrue(TEXT("The change made in flight is stored after completion"), Policy.ShouldFlush(1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamStatsFlushFailureTest, "SandwichSteam.Stats.FlushPolicy.FailureRetriesAfterWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamStatsFlushFailureTest::RunTest(const FString& Parameters)
{
	FSteamStatsFlushPolicy Policy(5.0);

	Policy.MarkUrgent(0.0);
	Policy.OnFlushStarted();
	Policy.OnFlushCompleted(false, 2.0);

	TestTrue(TEXT("A failed store keeps the data dirty"), Policy.IsDirty());
	TestFalse(TEXT("Retry waits for the window"), Policy.ShouldFlush(6.9));
	TestTrue(TEXT("Retry after the window"), Policy.ShouldFlush(7.0));

	Policy.Reset();
	TestFalse(TEXT("Reset clears the state"), Policy.IsDirty() || Policy.IsStoreInFlight());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
