// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SteamChangeBatch.h"
#include "Core/SteamPresenceRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamChangeBatchCoalesceTest, "SandwichSteam.Core.ChangeBatch.Coalesce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamChangeBatchCoalesceTest::RunTest(const FString& Parameters)
{
	TSteamChangeBatch<int64> Batch;
	TestTrue(TEXT("Starts empty"), Batch.IsEmpty());

	TestTrue(TEXT("The first Add asks for a flush"), Batch.Add(10, 0x1));
	TestFalse(TEXT("Later Adds join the pending flush"), Batch.Add(10, 0x4));
	TestFalse(TEXT("Another key joins too"), Batch.Add(20, 0x2));
	TestEqual(TEXT("Two keys"), Batch.Num(), 2);

	const TArray<TPair<int64, uint32>> Taken = Batch.Take();
	TestEqual(TEXT("Take returns every key once"), Taken.Num(), 2);
	for (const TPair<int64, uint32>& Entry : Taken)
	{
		TestEqual(TEXT("Flags are merged per key"), Entry.Value, Entry.Key == 10 ? 0x5u : 0x2u);
	}

	TestTrue(TEXT("Take empties the batch"), Batch.IsEmpty());
	TestTrue(TEXT("After a flush the next Add asks again"), Batch.Add(10, 0x1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPresenceRulesTest, "SandwichSteam.Core.PresenceRules.Limits",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamPresenceRulesTest::RunTest(const FString& Parameters)
{
	using namespace SandwichSteam::Presence;

	TestTrue(TEXT("Empty key"), CheckKey(FString()) == EIssue::EmptyKey);
	TestTrue(TEXT("Key of 64 bytes"), CheckKey(FString::ChrN(64, TEXT('k'))) == EIssue::None);
	TestTrue(TEXT("Key of 65 bytes"), CheckKey(FString::ChrN(65, TEXT('k'))) == EIssue::KeyTooLong);
	TestTrue(TEXT("Value of 256 bytes"), CheckValue(FString::ChrN(256, TEXT('v'))) == EIssue::None);
	TestTrue(TEXT("Value of 257 bytes"), CheckValue(FString::ChrN(257, TEXT('v'))) == EIssue::ValueTooLong);

	// The limits are bytes, not characters: 129 two-byte characters are 258 bytes.
	TestTrue(TEXT("Multi-byte value counts bytes"), CheckValue(FString::ChrN(129, TEXT('é'))) == EIssue::ValueTooLong);

	TestTrue(TEXT("steam_display is reserved"), IsReservedKey(TEXT("steam_display")));
	TestTrue(TEXT("connect is reserved"), IsReservedKey(TEXT("connect")));
	TestFalse(TEXT("map is free"), IsReservedKey(TEXT("map")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
