// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamProgressThrottle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamProgressThrottleStepsTest, "SandwichSteam.Achievements.ProgressThrottle.OnePerStep",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamProgressThrottleStepsTest::RunTest(const FString& Parameters)
{
	FSteamProgressThrottle Throttle(10);
	const FName Key(TEXT("ACH_TEST"));

	// 1..100 kills towards 100: the toast shows once per 10 percent step, and never at the maximum.
	int32 Shown = 0;
	for (int32 Current = 1; Current <= 100; ++Current)
	{
		Shown += Throttle.ShouldIndicate(Key, Current, 100) ? 1 : 0;
	}
	TestEqual(TEXT("Steps 0-9 show once each (0%..90%), 100 never"), Shown, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamProgressThrottleLimitsTest, "SandwichSteam.Achievements.ProgressThrottle.LimitsAndReset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamProgressThrottleLimitsTest::RunTest(const FString& Parameters)
{
	FSteamProgressThrottle Throttle(10);
	const FName Key(TEXT("ACH_TEST"));

	TestFalse(TEXT("No toast for zero progress"), Throttle.ShouldIndicate(Key, 0, 50));
	TestFalse(TEXT("No toast without a maximum"), Throttle.ShouldIndicate(Key, 5, 0));
	TestFalse(TEXT("No toast at the maximum"), Throttle.ShouldIndicate(Key, 50, 50));
	TestFalse(TEXT("No toast above the maximum"), Throttle.ShouldIndicate(Key, 60, 50));

	TestTrue(TEXT("First progress shows"), Throttle.ShouldIndicate(Key, 10, 50));
	TestFalse(TEXT("Same step does not show again"), Throttle.ShouldIndicate(Key, 11, 50));
	TestTrue(TEXT("Next step shows"), Throttle.ShouldIndicate(Key, 15, 50));

	// A stat reset lowers the remembered step, so the next rise shows again.
	TestFalse(TEXT("Falling progress shows nothing"), Throttle.ShouldIndicate(Key, 2, 50));
	TestTrue(TEXT("Rising again after a reset shows"), Throttle.ShouldIndicate(Key, 10, 50));

	Throttle.Forget(Key);
	TestTrue(TEXT("Forgotten achievements start over"), Throttle.ShouldIndicate(Key, 10, 50));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
