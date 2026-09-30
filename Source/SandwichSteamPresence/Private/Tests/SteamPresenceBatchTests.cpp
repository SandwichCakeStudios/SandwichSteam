// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamPresenceBatch.h"

using SandwichSteam::Presence::EIssue;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPresenceBatchDebounceTest, "SandwichSteam.Presence.Batch.Debounce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamPresenceBatchDebounceTest::RunTest(const FString& Parameters)
{
	FSteamPresenceBatch Batch;
	TestFalse(TEXT("Nothing pending at the start"), Batch.HasPending());
	TestTrue(TEXT("An empty flush is empty"), Batch.Flush().IsEmpty());

	// Several changes of one frame become one flush with the final values.
	TestTrue(TEXT("Set display"), Batch.Set(TEXT("steam_display"), TEXT("#Menu")) == EIssue::None);
	TestTrue(TEXT("Set map"), Batch.Set(TEXT("map"), TEXT("Harbor")) == EIssue::None);
	TestTrue(TEXT("Change display again"), Batch.Set(TEXT("steam_display"), TEXT("#InMatch")) == EIssue::None);

	FSteamPresenceBatch::FFlush First = Batch.Flush();
	TestFalse(TEXT("First flush does not clear"), First.bClearAll);
	TestEqual(TEXT("Two sets, not three"), First.Sets.Num(), 2);
	TestEqual(TEXT("No removals"), First.Removes.Num(), 0);
	TestFalse(TEXT("Flush empties the staging"), Batch.HasPending());

	// Setting what Steam already has costs nothing.
	Batch.Set(TEXT("map"), TEXT("Harbor"));
	TestTrue(TEXT("Pending until flushed"), Batch.HasPending());
	TestTrue(TEXT("Same value: nothing to send"), Batch.Flush().IsEmpty());

	// A key that is set and removed in the same frame never reaches Steam.
	Batch.Set(TEXT("score"), TEXT("10"));
	Batch.Remove(TEXT("score"));
	TestTrue(TEXT("Set then remove cancels out"), Batch.Flush().IsEmpty());

	// A removal of an applied key is sent.
	Batch.Remove(TEXT("map"));
	const FSteamPresenceBatch::FFlush Removal = Batch.Flush();
	TestEqual(TEXT("One removal"), Removal.Removes.Num(), 1);
	TestEqual(TEXT("It is the map"), Removal.Removes.Num() == 1 ? Removal.Removes[0] : FString(), FString(TEXT("map")));
	TestEqual(TEXT("One key left"), Batch.GetApplied().Num(), 1);

	// An empty value removes.
	Batch.Set(TEXT("steam_display"), FString());
	TestEqual(TEXT("Empty value removes the key"), Batch.Flush().Removes.Num(), 1);
	TestEqual(TEXT("Nothing left"), Batch.GetApplied().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPresenceBatchLimitsTest, "SandwichSteam.Presence.Batch.Limits",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamPresenceBatchLimitsTest::RunTest(const FString& Parameters)
{
	FSteamPresenceBatch Batch;
	TestTrue(TEXT("Empty key"), Batch.Set(FString(), TEXT("x")) == EIssue::EmptyKey);
	TestTrue(TEXT("Key over 64 bytes"), Batch.Set(FString::ChrN(65, TEXT('k')), TEXT("x")) == EIssue::KeyTooLong);
	TestTrue(TEXT("Value over 256 bytes"), Batch.Set(TEXT("k"), FString::ChrN(257, TEXT('v'))) == EIssue::ValueTooLong);
	TestFalse(TEXT("Refused changes stage nothing"), Batch.HasPending());

	for (int32 Index = 0; Index < SandwichSteam::Presence::MaxKeys; ++Index)
	{
		TestTrue(TEXT("Up to 30 keys"), Batch.Set(FString::Printf(TEXT("key%d"), Index), TEXT("v")) == EIssue::None);
	}
	TestTrue(TEXT("The 31st key is refused"), Batch.Set(TEXT("one_too_many"), TEXT("v")) == EIssue::TooManyKeys);
	TestTrue(TEXT("An existing key can still change"), Batch.Set(TEXT("key0"), TEXT("w")) == EIssue::None);
	TestEqual(TEXT("30 keys wanted"), Batch.GetDesiredCount(), SandwichSteam::Presence::MaxKeys);

	// Removing one makes room, also before the flush.
	Batch.Remove(TEXT("key1"));
	TestTrue(TEXT("Room after a removal"), Batch.Set(TEXT("one_too_many"), TEXT("v")) == EIssue::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPresenceBatchClearTest, "SandwichSteam.Presence.Batch.ClearAndRequeue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamPresenceBatchClearTest::RunTest(const FString& Parameters)
{
	FSteamPresenceBatch Batch;
	Batch.Set(TEXT("a"), TEXT("1"));
	Batch.Set(TEXT("b"), TEXT("2"));
	Batch.Flush();

	// Clear plus a new key in one frame: Steam is cleared once, then the new key is set.
	Batch.Clear();
	Batch.Set(TEXT("c"), TEXT("3"));
	const FSteamPresenceBatch::FFlush Cleared = Batch.Flush();
	TestTrue(TEXT("Steam is cleared"), Cleared.bClearAll);
	TestEqual(TEXT("Only the new key is set"), Cleared.Sets.Num(), 1);
	TestEqual(TEXT("Nothing is removed one by one"), Cleared.Removes.Num(), 0);
	TestEqual(TEXT("One key applied"), Batch.GetApplied().Num(), 1);

	// Clearing when nothing is applied needs no Steam call.
	Batch.Clear();
	Batch.Flush();
	Batch.Clear();
	TestFalse(TEXT("Nothing to clear"), Batch.Flush().bClearAll);

	// A reactivation sends every applied key again.
	Batch.Set(TEXT("x"), TEXT("1"));
	Batch.Set(TEXT("y"), TEXT("2"));
	Batch.Flush();
	Batch.RequeueApplied();
	TestTrue(TEXT("Requeued keys are pending"), Batch.HasPending());
	const FSteamPresenceBatch::FFlush Again = Batch.Flush();
	TestEqual(TEXT("Both keys are sent again"), Again.Sets.Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
