// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamRequestKey.h"

namespace
{
	FSteamLeaderboardQuery MakeQuery(ESteamLeaderboardRequestType Type, int32 Start, int32 End)
	{
		FSteamLeaderboardQuery Query;
		Query.Type = Type;
		Query.RangeStart = Start;
		Query.RangeEnd = End;
		return Query;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamRequestKeyEqualityTest, "SandwichSteam.Leaderboards.RequestKey.Equality",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamRequestKeyEqualityTest::RunTest(const FString& Parameters)
{
	const FName Board(TEXT("HighScore"));
	const FSteamLeaderboardQuery Top10 = SandwichSteam::Leaderboards::NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::Global, 1, 10));

	const FSteamRequestKey A = FSteamRequestKey::ForDownload(Board, Top10);
	const FSteamRequestKey B = FSteamRequestKey::ForDownload(Board, Top10);
	TestTrue(TEXT("Identical requests have equal keys"), A == B);
	TestEqual(TEXT("Identical requests hash the same"), GetTypeHash(A), GetTypeHash(B));

	TestFalse(TEXT("Other leaderboard"), A == FSteamRequestKey::ForDownload(FName(TEXT("Other")), Top10));
	TestFalse(TEXT("Other range"), A == FSteamRequestKey::ForDownload(Board, SandwichSteam::Leaderboards::NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::Global, 1, 20))));
	TestFalse(TEXT("Other type"), A == FSteamRequestKey::ForDownload(Board, SandwichSteam::Leaderboards::NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::AroundUser, 1, 10))));

	FSteamLeaderboardQuery UsersA = MakeQuery(ESteamLeaderboardRequestType::Users, 0, 0);
	UsersA.Users = { FSteamId(1), FSteamId(2) };
	FSteamLeaderboardQuery UsersB = UsersA;
	UsersB.Users.Add(FSteamId(3));
	TestFalse(TEXT("Other user list"), FSteamRequestKey::ForDownload(Board, SandwichSteam::Leaderboards::NormalizeQuery(UsersA)) == FSteamRequestKey::ForDownload(Board, SandwichSteam::Leaderboards::NormalizeQuery(UsersB)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamRequestKeyNormalizeTest, "SandwichSteam.Leaderboards.RequestKey.Normalize",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamRequestKeyNormalizeTest::RunTest(const FString& Parameters)
{
	using namespace SandwichSteam::Leaderboards;

	const FSteamLeaderboardQuery Huge = NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::Global, -5, 100000));
	TestEqual(TEXT("Global starts at rank 1"), Huge.RangeStart, 1);
	TestEqual(TEXT("Global is limited to one page"), Huge.RangeEnd - Huge.RangeStart + 1, MaxPageSize);

	const FSteamLeaderboardQuery Reversed = NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::AroundUser, 5, -4));
	TestEqual(TEXT("Around user orders the range (start)"), Reversed.RangeStart, -4);
	TestEqual(TEXT("Around user orders the range (end)"), Reversed.RangeEnd, 5);

	const FSteamLeaderboardQuery Friends = NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::Friends, 7, 9));
	TestEqual(TEXT("Friends ignores the range (start)"), Friends.RangeStart, 0);
	TestEqual(TEXT("Friends ignores the range (end)"), Friends.RangeEnd, 0);

	FSteamLeaderboardQuery Users = MakeQuery(ESteamLeaderboardRequestType::Users, 3, 4);
	Users.Users.Add(FSteamId(0)); // invalid, dropped
	for (int32 Index = 1; Index <= 150; ++Index)
	{
		Users.Users.Add(FSteamId(Index));
	}
	const FSteamLeaderboardQuery NormalizedUsers = NormalizeQuery(Users);
	TestEqual(TEXT("Users drops invalid ids and is limited to one page"), NormalizedUsers.Users.Num(), MaxPageSize);
	TestEqual(TEXT("Users ignores the range"), NormalizedUsers.RangeStart, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamRequestCoalescerTest, "SandwichSteam.Leaderboards.RequestKey.Coalescer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamRequestCoalescerTest::RunTest(const FString& Parameters)
{
	const FName Board(TEXT("HighScore"));
	const FSteamRequestKey Top10 = FSteamRequestKey::ForDownload(Board, SandwichSteam::Leaderboards::NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::Global, 1, 10)));
	const FSteamRequestKey Top20 = FSteamRequestKey::ForDownload(Board, SandwichSteam::Leaderboards::NormalizeQuery(MakeQuery(ESteamLeaderboardRequestType::Global, 1, 20)));

	TSteamRequestCoalescer<FSteamRequestKey, int32> Coalescer;

	// Two identical requests: the first starts the Steam call, the second only waits. A different request starts its own.
	TestTrue(TEXT("First request starts a call"), Coalescer.Join(Top10, 1));
	TestFalse(TEXT("Identical request joins"), Coalescer.Join(Top10, 2));
	TestTrue(TEXT("Different request starts its own call"), Coalescer.Join(Top20, 3));
	TestEqual(TEXT("Two distinct requests in flight"), Coalescer.Num(), 2);

	const TArray<int32> Waiters = Coalescer.Take(Top10);
	TestEqual(TEXT("Both callers of the shared call are answered"), Waiters.Num(), 2);
	TestEqual(TEXT("Waiters keep their join order (first)"), Waiters[0], 1);
	TestEqual(TEXT("Waiters keep their join order (second)"), Waiters[1], 2);
	TestFalse(TEXT("Finished request is forgotten"), Coalescer.IsInFlight(Top10));
	TestTrue(TEXT("Other request is untouched"), Coalescer.IsInFlight(Top20));

	TestTrue(TEXT("After completion the same request starts a new call"), Coalescer.Join(Top10, 4));
	TestEqual(TEXT("Taking an unknown key returns nothing"), Coalescer.Take(FSteamRequestKey::ForDownload(FName(TEXT("Nope")), FSteamLeaderboardQuery())).Num(), 0);

	Coalescer.Reset();
	TestEqual(TEXT("Reset drops everything"), Coalescer.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
