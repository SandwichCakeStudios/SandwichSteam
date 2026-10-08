// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Settings/SteamIniWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIniWriterIdempotentTest, "SandwichSteam.Editor.IniWriter.Idempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamIniWriterIdempotentTest::RunTest(const FString& Parameters)
{
	const TArray<FSteamIniEntry> Entries = FSteamIniWriter::BuildRequiredEntries(480);

	const TArray<FString> Inputs = {
		TEXT(""),
		TEXT("[/Script/EngineSettings.GameMapsSettings]\nGameDefaultMap=/Game/Map\n"),
		TEXT("[OnlineSubsystem]\nDefaultPlatformService=Null\n\n[OnlineSubsystemSteam]\nbEnabled=false\nSteamDevAppId=1\n"),
		TEXT("[OnlineSubsystem]\r\nDefaultPlatformService=Steam\r\n"),
		TEXT("[A]\nx=1")
	};

	for (const FString& Input : Inputs)
	{
		const FString Once = FSteamIniWriter::Apply(Input, Entries);
		const FString Twice = FSteamIniWriter::Apply(Once, Entries);
		TestEqual(TEXT("Applying twice equals applying once"), Twice, Once);

		for (const FSteamIniChange& Change : FSteamIniWriter::Diff(Once, Entries))
		{
			TestTrue(TEXT("Nothing left to change after Apply"), Change.Change == ESteamIniChange::Unchanged);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIniWriterContentTest, "SandwichSteam.Editor.IniWriter.Content",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamIniWriterContentTest::RunTest(const FString& Parameters)
{
	const TArray<FSteamIniEntry> Entries = {
		{ TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), TEXT("Steam") },
		{ TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), TEXT("480") }
	};

	// Empty input: sections are created.
	TestEqual(TEXT("Empty input"),
		FSteamIniWriter::Apply(FString(), Entries),
		FString(TEXT("[OnlineSubsystem]\nDefaultPlatformService=Steam\n\n[OnlineSubsystemSteam]\nSteamDevAppId=480\n")));

	// Existing values are replaced, unrelated content and comments are kept, missing keys go to the end of their section.
	const FString Existing = TEXT("[OnlineSubsystem]\n; keep me\nDefaultPlatformService=Null\nOther=1\n\n[Other]\nA=B\n\n[OnlineSubsystemSteam]\nbEnabled=true\n");
	TestEqual(TEXT("Existing content"),
		FSteamIniWriter::Apply(Existing, Entries),
		FString(TEXT("[OnlineSubsystem]\n; keep me\nDefaultPlatformService=Steam\nOther=1\n\n[Other]\nA=B\n\n[OnlineSubsystemSteam]\nbEnabled=true\nSteamDevAppId=480\n")));

	// Nothing to change: the input is returned untouched, even without a trailing newline.
	const FString AlreadyDone = TEXT("[OnlineSubsystem]\nDefaultPlatformService=steam\n[OnlineSubsystemSteam]\nSteamDevAppId=480");
	TestEqual(TEXT("Unchanged input"), FSteamIniWriter::Apply(AlreadyDone, Entries), AlreadyDone);

	// Line endings are preserved.
	TestEqual(TEXT("CRLF preserved"),
		FSteamIniWriter::Apply(TEXT("[OnlineSubsystem]\r\nDefaultPlatformService=Null\r\n"), TArray<FSteamIniEntry>{ Entries[0] }),
		FString(TEXT("[OnlineSubsystem]\r\nDefaultPlatformService=Steam\r\n")));

	// Diff reports what will happen.
	const TArray<FSteamIniChange> Changes = FSteamIniWriter::Diff(TEXT("[OnlineSubsystem]\nDefaultPlatformService=Null\n"), Entries);
	TestEqual(TEXT("Diff count"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestTrue(TEXT("First entry modified"), Changes[0].Change == ESteamIniChange::Modified && Changes[0].OldValue == TEXT("Null"));
		TestTrue(TEXT("Second entry added"), Changes[1].Change == ESteamIniChange::Added);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIniWriterSessionsTest, "SandwichSteam.Editor.IniWriter.Sessions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamIniWriterSessionsTest::RunTest(const FString& Parameters)
{
	const TArray<FSteamIniEntry> Plain = FSteamIniWriter::BuildRequiredEntries(480);
	const TArray<FSteamIniEntry> WithSessions = FSteamIniWriter::BuildRequiredEntries(480, /*bWithSessions*/ true);
	TestTrue(TEXT("Sessions add entries"), WithSessions.Num() > Plain.Num());

	// Idempotent, also on an ini that already has other net driver definitions (array elements are matched by value).
	const TArray<FString> Inputs = {
		TEXT(""),
		TEXT("[/Script/Engine.Engine]\n+NetDriverDefinitions=(DefName=\"GameNetDriver\",DriverClassName=\"/Script/Other.Driver\")\n"),
		TEXT("[OnlineSubsystemSteam]\nbEnabled=true\n")
	};

	for (const FString& Input : Inputs)
	{
		const FString Once = FSteamIniWriter::Apply(Input, WithSessions);
		TestEqual(TEXT("Applying twice equals applying once"), FSteamIniWriter::Apply(Once, WithSessions), Once);
		for (const FSteamIniChange& Change : FSteamIniWriter::Diff(Once, WithSessions))
		{
			TestTrue(TEXT("Nothing left to change after Apply"), Change.Change == ESteamIniChange::Unchanged);
		}
	}

	// An existing array element with the same key but another value is kept, ours is added next to it.
	const FString Existing = TEXT("[/Script/Engine.Engine]\n+NetDriverDefinitions=(DefName=\"Mine\")\n");
	const TArray<FSteamIniEntry> Element = {
		{ TEXT("/Script/Engine.Engine"), TEXT("+NetDriverDefinitions"), TEXT("(DefName=\"Ours\")") }
	};
	TestEqual(TEXT("Array element added, not replaced"),
		FSteamIniWriter::Apply(Existing, Element),
		FString(TEXT("[/Script/Engine.Engine]\n+NetDriverDefinitions=(DefName=\"Mine\")\n+NetDriverDefinitions=(DefName=\"Ours\")\n")));

	const TArray<FSteamIniChange> Changes = FSteamIniWriter::Diff(Existing, Element);
	TestTrue(TEXT("Diff reports an added element"), Changes.Num() == 1 && Changes[0].Change == ESteamIniChange::Added);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIniWriterVoiceTest, "SandwichSteam.Editor.IniWriter.Voice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamIniWriterVoiceTest::RunTest(const FString& Parameters)
{
	const TArray<FSteamIniEntry> Plain = FSteamIniWriter::BuildRequiredEntries(480);
	const TArray<FSteamIniEntry> WithVoice = FSteamIniWriter::BuildRequiredEntries(480, /*bWithSessions*/ false, /*bWithVoice*/ true);
	TestEqual(TEXT("Voice adds exactly two engine entries"), WithVoice.Num(), Plain.Num() + 2);
	TestTrue(TEXT("The engine voice module is switched on"), WithVoice.ContainsByPredicate([](const FSteamIniEntry& Entry)
	{
		return Entry.Section == TEXT("Voice") && Entry.Key == TEXT("bEnabled") && Entry.Value == TEXT("true");
	}));
	TestTrue(TEXT("The entry is bHasVoiceEnabled"), WithVoice.ContainsByPredicate([](const FSteamIniEntry& Entry)
	{
		return Entry.Section == TEXT("OnlineSubsystem") && Entry.Key == TEXT("bHasVoiceEnabled") && Entry.Value == TEXT("true");
	}));

	TestEqual(TEXT("No game ini entries without voice"), FSteamIniWriter::BuildGameEntries(false).Num(), 0);
	const TArray<FSteamIniEntry> Game = FSteamIniWriter::BuildGameEntries(true);
	TestTrue(TEXT("Voice asks the game session for push to talk"), Game.Num() == 1 && Game[0].Section == TEXT("/Script/Engine.GameSession") && Game[0].Key == TEXT("bRequiresPushToTalk") && Game[0].Value == TEXT("true"));

	// Idempotent, and a wrong existing value is replaced.
	const FString Once = FSteamIniWriter::Apply(TEXT("[/Script/Engine.GameSession]\nbRequiresPushToTalk=False\n"), Game);
	TestEqual(TEXT("Replaced"), Once, FString(TEXT("[/Script/Engine.GameSession]\nbRequiresPushToTalk=true\n")));
	TestEqual(TEXT("Applying twice equals applying once"), FSteamIniWriter::Apply(Once, Game), Once);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIniWriterRelaunchTest, "SandwichSteam.Editor.IniWriter.Relaunch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamIniWriterRelaunchTest::RunTest(const FString& Parameters)
{
	const auto IsRelaunchEntry = [](const FSteamIniEntry& Entry)
	{
		return Entry.Section == TEXT("OnlineSubsystemSteam") && Entry.Key == TEXT("bRelaunchInSteam");
	};

	const TArray<FSteamIniEntry> Default = FSteamIniWriter::BuildRequiredEntries(480);
	const FSteamIniEntry* Relaunch = Default.FindByPredicate(IsRelaunchEntry);
	TestTrue(TEXT("Written by default, as false"), Relaunch && Relaunch->Value == TEXT("false"));

	const TArray<FSteamIniEntry> Without = FSteamIniWriter::BuildRequiredEntries(480, /*bWithSessions*/ false, /*bWithVoice*/ false, /*bWithRelaunchOff*/ false);
	TestFalse(TEXT("Optional: left out when turned off"), Without.ContainsByPredicate(IsRelaunchEntry));
	TestEqual(TEXT("Only that entry is left out"), Without.Num(), Default.Num() - 1);

	// Turned off, a project's own value is neither reported nor touched.
	const FString Own = TEXT("[OnlineSubsystemSteam]\nbRelaunchInSteam=true\n");
	for (const FSteamIniChange& Change : FSteamIniWriter::Diff(Own, Without))
	{
		TestFalse(TEXT("The key is not in the diff"), IsRelaunchEntry(Change.Entry));
	}
	TestTrue(TEXT("The project's value is kept"), FSteamIniWriter::Apply(Own, Without).Contains(TEXT("bRelaunchInSteam=true")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamIniWriterNetworkTuningTest, "SandwichSteam.Editor.IniWriter.NetworkTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamIniWriterNetworkTuningTest::RunTest(const FString& Parameters)
{
	const FSteamNetworkTuning Tuning;
	const TArray<FSteamIniEntry> Engine = FSteamIniWriter::BuildNetworkEngineEntries(Tuning);
	const TArray<FSteamIniEntry> Game = FSteamIniWriter::BuildNetworkGameEntries(Tuning);

	const auto ValueOf = [](const TArray<FSteamIniEntry>& Entries, const TCHAR* Section, const TCHAR* Key)
	{
		const FSteamIniEntry* Found = Entries.FindByPredicate([Section, Key](const FSteamIniEntry& Entry)
		{
			return Entry.Section == Section && Entry.Key == Key;
		});
		return Found ? Found->Value : FString(TEXT("<missing>"));
	};

	// The client limit (Player, DefaultEngine.ini) has to match the server limit, so they share one value.
	TestEqual(TEXT("MaxClientRate"), ValueOf(Engine, TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"), TEXT("MaxClientRate")), FString(TEXT("200000")));
	TestEqual(TEXT("MaxInternetClientRate"), ValueOf(Engine, TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"), TEXT("MaxInternetClientRate")), FString(TEXT("200000")));
	TestEqual(TEXT("InitialConnectTimeout"), ValueOf(Engine, TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"), TEXT("InitialConnectTimeout")), FString(TEXT("60.0")));
	TestEqual(TEXT("ConfiguredInternetSpeed is in DefaultEngine.ini"), ValueOf(Engine, TEXT("/Script/Engine.Player"), TEXT("ConfiguredInternetSpeed")), FString(TEXT("200000")));
	TestEqual(TEXT("ConfiguredLanSpeed is in DefaultEngine.ini"), ValueOf(Engine, TEXT("/Script/Engine.Player"), TEXT("ConfiguredLanSpeed")), FString(TEXT("200000")));

	TestEqual(TEXT("TotalNetBandwidth is the raw total"), ValueOf(Game, TEXT("/Script/Engine.GameNetworkManager"), TEXT("TotalNetBandwidth")), FString(TEXT("800000")));
	TestEqual(TEXT("MaxDynamicBandwidth follows the per-client value"), ValueOf(Game, TEXT("/Script/Engine.GameNetworkManager"), TEXT("MaxDynamicBandwidth")), FString(TEXT("200000")));
	TestEqual(TEXT("MinDynamicBandwidth"), ValueOf(Game, TEXT("/Script/Engine.GameNetworkManager"), TEXT("MinDynamicBandwidth")), FString(TEXT("20000")));
	TestEqual(TEXT("No Player entry in DefaultGame.ini"), ValueOf(Game, TEXT("/Script/Engine.Player"), TEXT("ConfiguredInternetSpeed")), FString(TEXT("<missing>")));

	// A project's own value is replaced (and reported as Modified), then Apply is idempotent.
	const FString Own = TEXT("[/Script/OnlineSubsystemUtils.IpNetDriver]\nMaxClientRate=15000\n");
	const TArray<FSteamIniChange> Changes = FSteamIniWriter::Diff(Own, Engine);
	TestEqual(TEXT("MaxClientRate is Modified"), Changes[0].Change, ESteamIniChange::Modified);
	TestEqual(TEXT("Old value is reported"), Changes[0].OldValue, FString(TEXT("15000")));
	const FString Once = FSteamIniWriter::Apply(Own, Engine);
	TestEqual(TEXT("Applying twice equals applying once"), FSteamIniWriter::Apply(Once, Engine), Once);

	FString Problem;
	TestTrue(TEXT("Defaults are consistent"), FSteamIniWriter::IsNetworkTuningConsistent(Tuning, Problem));
	FSteamNetworkTuning MinTooHigh = Tuning;
	MinTooHigh.MinDynamicBandwidth = Tuning.BandwidthPerClient + 1;
	TestFalse(TEXT("Min above per-client is flagged"), FSteamIniWriter::IsNetworkTuningConsistent(MinTooHigh, Problem));
	FSteamNetworkTuning ClientAboveTotal = Tuning;
	ClientAboveTotal.TotalBandwidth = Tuning.BandwidthPerClient - 1;
	TestFalse(TEXT("Per-client above total is flagged"), FSteamIniWriter::IsNetworkTuningConsistent(ClientAboveTotal, Problem));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
