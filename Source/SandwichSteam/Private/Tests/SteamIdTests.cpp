// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SteamId.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIdRoundTripTest, "SandwichSteam.Core.SteamId.RoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamIdRoundTripTest::RunTest(const FString& Parameters)
{
	// Individual account 22202 in the public universe.
	const FSteamId User(76561197960287930ll);
	TestTrue(TEXT("User is valid"), User.IsValid());
	TestEqual(TEXT("User ToString"), User.ToString(), FString(TEXT("76561197960287930")));
	TestEqual(TEXT("User ToSteamID3"), User.ToSteamID3(), FString(TEXT("[U:1:22202]")));

	FSteamId Parsed;
	TestTrue(TEXT("Parse SteamID64"), FSteamId::FromString(TEXT("76561197960287930"), Parsed));
	TestTrue(TEXT("SteamID64 round trip"), Parsed == User);

	TestTrue(TEXT("Parse SteamID3"), FSteamId::FromString(TEXT("[U:1:22202]"), Parsed));
	TestTrue(TEXT("SteamID3 round trip"), Parsed == User);

	TestTrue(TEXT("Parse with whitespace"), FSteamId::FromString(TEXT("  76561197960287930 "), Parsed));
	TestTrue(TEXT("Whitespace round trip"), Parsed == User);

	// Lobby: chat account type with the lobby instance flag.
	const int64 LobbyValue = (1ll << 56) | (8ll << 52) | (0x40000ll << 32) | 123ll;
	const FSteamId Lobby(LobbyValue);
	TestEqual(TEXT("Lobby ToSteamID3"), Lobby.ToSteamID3(), FString(TEXT("[L:1:123]")));
	TestTrue(TEXT("Parse lobby SteamID3"), FSteamId::FromString(TEXT("[L:1:123]"), Parsed));
	TestTrue(TEXT("Lobby round trip"), Parsed == Lobby);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIdInvalidInputTest, "SandwichSteam.Core.SteamId.InvalidInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamIdInvalidInputTest::RunTest(const FString& Parameters)
{
	const FSteamId Invalid;
	TestFalse(TEXT("Default is invalid"), Invalid.IsValid());
	TestTrue(TEXT("Invalid ToString is empty"), Invalid.ToString().IsEmpty());
	TestTrue(TEXT("Invalid ToSteamID3 is empty"), Invalid.ToSteamID3().IsEmpty());

	const TCHAR* BadInputs[] = { TEXT(""), TEXT("   "), TEXT("abc"), TEXT("0"), TEXT("[U:1]"), TEXT("[X:1:5]"), TEXT("[U:1:abc]"), TEXT("[U:0:5]"), TEXT("[U:1:99999999999]"), TEXT("123456789012345678901234") };
	for (const TCHAR* Bad : BadInputs)
	{
		FSteamId Out(42);
		TestFalse(FString::Printf(TEXT("'%s' is rejected"), Bad), FSteamId::FromString(Bad, Out));
		TestFalse(FString::Printf(TEXT("'%s' leaves an invalid id"), Bad), Out.IsValid());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIdEqualityTest, "SandwichSteam.Core.SteamId.EqualityAndHash",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamIdEqualityTest::RunTest(const FString& Parameters)
{
	const FSteamId A(76561197960287930ll);
	const FSteamId B(76561197960287930ll);
	const FSteamId C(76561197960287931ll);

	TestTrue(TEXT("Equal values are equal"), A == B);
	TestTrue(TEXT("Different values differ"), A != C);
	TestEqual(TEXT("Equal ids hash equally"), GetTypeHash(A), GetTypeHash(B));

	TSet<FSteamId> Set;
	Set.Add(A);
	Set.Add(B);
	Set.Add(C);
	TestEqual(TEXT("Set de-duplicates"), Set.Num(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
