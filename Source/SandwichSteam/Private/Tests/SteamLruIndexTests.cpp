// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SteamLruIndex.h"
#include "Features/Utility/SteamLanguage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamLruEvictsOldestTest, "SandwichSteam.User.LruIndex.EvictsLeastRecentlyUsed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamLruEvictsOldestTest::RunTest(const FString& Parameters)
{
	TSteamLruIndex<int32> Lru(3);
	Lru.Touch(1);
	Lru.Touch(2);
	Lru.Touch(3);
	Lru.Touch(1); // 2 is now the oldest.

	TArray<int32> Evicted;
	Lru.Evict(Evicted);
	TestEqual(TEXT("Nothing is evicted at capacity"), Evicted.Num(), 0);

	Lru.Touch(4);
	Lru.Evict(Evicted);
	TestEqual(TEXT("One key evicted"), Evicted.Num(), 1);
	TestTrue(TEXT("Key 2 (least recently used) was evicted"), Evicted.Num() == 1 && Evicted[0] == 2);
	TestEqual(TEXT("Size is back at capacity"), Lru.Num(), 3);
	TestTrue(TEXT("Touched key survives"), Lru.Contains(1));
	TestFalse(TEXT("Evicted key is gone"), Lru.Contains(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamLruShrinkAndRemoveTest, "SandwichSteam.User.LruIndex.ShrinkAndRemove",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamLruShrinkAndRemoveTest::RunTest(const FString& Parameters)
{
	TSteamLruIndex<int32> Lru(5);
	for (int32 Key = 1; Key <= 5; ++Key)
	{
		Lru.Touch(Key);
	}

	Lru.SetCapacity(2);
	TArray<int32> Evicted;
	Lru.Evict(Evicted);
	TestEqual(TEXT("Shrinking evicts the excess"), Evicted.Num(), 3);
	TestTrue(TEXT("Oldest keys leave first"), Evicted.Num() == 3 && Evicted[0] == 1 && Evicted[1] == 2 && Evicted[2] == 3);
	TestTrue(TEXT("Newest keys stay"), Lru.Contains(4) && Lru.Contains(5));

	TestTrue(TEXT("Remove reports a tracked key"), Lru.Remove(4));
	TestFalse(TEXT("Remove reports an unknown key"), Lru.Remove(4));

	Lru.SetCapacity(0);
	TestEqual(TEXT("Capacity never drops below 1"), Lru.GetCapacity(), 1);

	Lru.Reset();
	TestEqual(TEXT("Reset clears the index"), Lru.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamLanguageMappingTest, "SandwichSteam.Utility.Language.SteamToCulture",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamLanguageMappingTest::RunTest(const FString& Parameters)
{
	FString Culture;
	TestTrue(TEXT("english maps"), SandwichSteam::SteamLanguageToCulture(TEXT("english"), Culture));
	TestEqual(TEXT("english -> en"), Culture, FString(TEXT("en")));

	TestTrue(TEXT("Case is ignored"), SandwichSteam::SteamLanguageToCulture(TEXT("SChinese"), Culture));
	TestEqual(TEXT("schinese -> zh-Hans"), Culture, FString(TEXT("zh-Hans")));

	TestTrue(TEXT("brazilian maps"), SandwichSteam::SteamLanguageToCulture(TEXT("brazilian"), Culture));
	TestEqual(TEXT("brazilian -> pt-BR"), Culture, FString(TEXT("pt-BR")));

	TestTrue(TEXT("koreana maps"), SandwichSteam::SteamLanguageToCulture(TEXT("koreana"), Culture));
	TestEqual(TEXT("koreana -> ko"), Culture, FString(TEXT("ko")));

	TestFalse(TEXT("Unknown language fails"), SandwichSteam::SteamLanguageToCulture(TEXT("klingon"), Culture));
	TestTrue(TEXT("Unknown language clears the output"), Culture.IsEmpty());

	TestFalse(TEXT("Empty language fails"), SandwichSteam::SteamLanguageToCulture(FString(), Culture));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
