// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamCloudSaveFormat.h"

namespace
{
	TArray<uint8> MakePayload(int32 Count)
	{
		TArray<uint8> Payload;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Payload.Add(static_cast<uint8>(Index * 7 + 3));
		}
		return Payload;
	}

	FSteamCloudSaveHeader MakeHeader(int64 Time, uint32 Crc)
	{
		FSteamCloudSaveHeader Header;
		Header.Version = FSteamCloudSaveFormat::CurrentVersion;
		Header.UnixTime = Time;
		Header.Crc32 = Crc;
		Header.PayloadSize = 10;
		return Header;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCloudFormatRoundTripTest, "SandwichSteam.Cloud.Format.RoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamCloudFormatRoundTripTest::RunTest(const FString& Parameters)
{
	for (const int32 Size : { 0, 1, 1000 })
	{
		const TArray<uint8> Payload = MakePayload(Size);
		TArray<uint8> Bytes;
		FSteamCloudSaveFormat::Encode(Payload, 1700000000, Bytes);
		TestEqual(TEXT("Header plus payload"), Bytes.Num(), FSteamCloudSaveFormat::HeaderSize + Size);

		FSteamCloudSaveHeader Header;
		TArray<uint8> Decoded;
		TestTrue(TEXT("Decodes"), FSteamCloudSaveFormat::Decode(Bytes, Header, Decoded) == ESteamCloudDecode::Ok);
		TestEqual(TEXT("Time"), Header.UnixTime, static_cast<int64>(1700000000));
		TestEqual(TEXT("Payload size"), static_cast<int32>(Header.PayloadSize), Size);
		TestTrue(TEXT("Payload is identical"), Decoded == Payload);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCloudFormatCorruptTest, "SandwichSteam.Cloud.Format.Corrupt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamCloudFormatCorruptTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Payload = MakePayload(64);
	TArray<uint8> Good;
	FSteamCloudSaveFormat::Encode(Payload, 42, Good);

	FSteamCloudSaveHeader Header;
	TArray<uint8> Out;

	TArray<uint8> Flipped = Good;
	Flipped[FSteamCloudSaveFormat::HeaderSize + 10] ^= 0x01;
	TestTrue(TEXT("A flipped payload bit fails the CRC"), FSteamCloudSaveFormat::Decode(Flipped, Header, Out) == ESteamCloudDecode::BadCrc);
	TestTrue(TEXT("Nothing is returned for corrupt data"), Out.IsEmpty());

	TArray<uint8> Truncated = Good;
	Truncated.SetNum(Truncated.Num() - 5);
	TestTrue(TEXT("Truncated file"), FSteamCloudSaveFormat::Decode(Truncated, Header, Out) == ESteamCloudDecode::BadSize);

	TArray<uint8> Short = Good;
	Short.SetNum(10);
	TestTrue(TEXT("Shorter than the header"), FSteamCloudSaveFormat::Decode(Short, Header, Out) == ESteamCloudDecode::TooShort);

	TArray<uint8> Foreign = Good;
	Foreign[0] ^= 0xFF;
	TestTrue(TEXT("Wrong magic"), FSteamCloudSaveFormat::Decode(Foreign, Header, Out) == ESteamCloudDecode::BadMagic);

	TArray<uint8> Newer = Good;
	Newer[4] = 99;
	TestTrue(TEXT("Unknown version"), FSteamCloudSaveFormat::Decode(Newer, Header, Out) == ESteamCloudDecode::UnsupportedVersion);

	TestTrue(TEXT("Empty input"), FSteamCloudSaveFormat::Decode(TConstArrayView<uint8>(), Header, Out) == ESteamCloudDecode::TooShort);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCloudConflictTest, "SandwichSteam.Cloud.Format.Conflict",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamCloudConflictTest::RunTest(const FString& Parameters)
{
	const FSteamCloudSaveHeader Old = MakeHeader(100, 1);
	const FSteamCloudSaveHeader New = MakeHeader(200, 2);
	const FSteamCloudSaveHeader NewCopy = MakeHeader(200, 2);
	const FSteamCloudSaveHeader TieA = MakeHeader(200, 5);
	const FSteamCloudSaveHeader TieB = MakeHeader(200, 6);

	for (const ESteamCloudConflictPolicy Policy : { ESteamCloudConflictPolicy::NewestWins, ESteamCloudConflictPolicy::PreferLocal, ESteamCloudConflictPolicy::PreferCloud, ESteamCloudConflictPolicy::Ask })
	{
		TestTrue(TEXT("Nothing anywhere"), FSteamCloudSaveFormat::Resolve(Policy, nullptr, nullptr) == ESteamCloudResolution::NothingFound);
		TestTrue(TEXT("Only local"), FSteamCloudSaveFormat::Resolve(Policy, &Old, nullptr) == ESteamCloudResolution::UseLocal);
		TestTrue(TEXT("Only cloud (corrupt or missing local)"), FSteamCloudSaveFormat::Resolve(Policy, nullptr, &Old) == ESteamCloudResolution::UseCloud);
		TestTrue(TEXT("Identical copies are in sync"), FSteamCloudSaveFormat::Resolve(Policy, &New, &NewCopy) == ESteamCloudResolution::InSync);
	}

	using P = ESteamCloudConflictPolicy;
	using R = ESteamCloudResolution;
	TestTrue(TEXT("Newest wins: cloud newer"), FSteamCloudSaveFormat::Resolve(P::NewestWins, &Old, &New) == R::UseCloud);
	TestTrue(TEXT("Newest wins: local newer"), FSteamCloudSaveFormat::Resolve(P::NewestWins, &New, &Old) == R::UseLocal);
	TestTrue(TEXT("Newest wins: a tie keeps local"), FSteamCloudSaveFormat::Resolve(P::NewestWins, &TieA, &TieB) == R::UseLocal);
	TestTrue(TEXT("Prefer local"), FSteamCloudSaveFormat::Resolve(P::PreferLocal, &Old, &New) == R::UseLocal);
	TestTrue(TEXT("Prefer cloud"), FSteamCloudSaveFormat::Resolve(P::PreferCloud, &New, &Old) == R::UseCloud);
	TestTrue(TEXT("Ask"), FSteamCloudSaveFormat::Resolve(P::Ask, &Old, &New) == R::Ask);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCloudSlotNameTest, "SandwichSteam.Cloud.Format.SlotName",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamCloudSlotNameTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Plain"), FSteamCloudSaveFormat::IsValidSlotName(TEXT("Slot_1-a")));
	TestFalse(TEXT("Empty"), FSteamCloudSaveFormat::IsValidSlotName(FString()));
	TestFalse(TEXT("Path separator"), FSteamCloudSaveFormat::IsValidSlotName(TEXT("../evil")));
	TestFalse(TEXT("Space"), FSteamCloudSaveFormat::IsValidSlotName(TEXT("my slot")));
	TestFalse(TEXT("Too long"), FSteamCloudSaveFormat::IsValidSlotName(FString::ChrN(101, TEXT('a'))));
	TestTrue(TEXT("Longest"), FSteamCloudSaveFormat::IsValidSlotName(FString::ChrN(100, TEXT('a'))));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
