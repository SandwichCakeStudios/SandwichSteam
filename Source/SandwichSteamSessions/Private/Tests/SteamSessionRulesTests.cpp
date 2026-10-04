// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamSessionRules.h"

using namespace SandwichSteam::Sessions;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamJoinDecisionTest, "SandwichSteam.Sessions.Rules.JoinDecision",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamJoinDecisionTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		ESteamJoinInMatchPolicy Policy;
		bool bInMatch;
		bool bAutoJoin;
		bool bAlreadyInTarget;
		ESteamJoinAction Expected;
		const TCHAR* Name;
	};

	const FCase Cases[] =
	{
		// Not in a match: the policy does not matter.
		{ ESteamJoinInMatchPolicy::AskGame,   false, true,  false, ESteamJoinAction::JoinNow,      TEXT("menu, auto join") },
		{ ESteamJoinInMatchPolicy::Ignore,    false, true,  false, ESteamJoinAction::JoinNow,      TEXT("menu, auto join, policy ignore") },
		{ ESteamJoinInMatchPolicy::AutoLeave, false, false, false, ESteamJoinAction::AskGame,      TEXT("menu, game decides") },
		// In a match.
		{ ESteamJoinInMatchPolicy::AutoLeave, true,  true,  false, ESteamJoinAction::LeaveAndJoin, TEXT("match, auto leave") },
		{ ESteamJoinInMatchPolicy::AutoLeave, true,  false, false, ESteamJoinAction::AskGame,      TEXT("match, auto leave but game decides") },
		{ ESteamJoinInMatchPolicy::AskGame,   true,  true,  false, ESteamJoinAction::AskGame,      TEXT("match, ask game") },
		{ ESteamJoinInMatchPolicy::AskGame,   true,  false, false, ESteamJoinAction::AskGame,      TEXT("match, ask game, no auto") },
		{ ESteamJoinInMatchPolicy::Ignore,    true,  true,  false, ESteamJoinAction::Ignore,       TEXT("match, ignore") },
		{ ESteamJoinInMatchPolicy::Ignore,    true,  false, false, ESteamJoinAction::Ignore,       TEXT("match, ignore, no auto") },
		// Already in the session that is asked for.
		{ ESteamJoinInMatchPolicy::AutoLeave, true,  true,  true,  ESteamJoinAction::Ignore,       TEXT("same session, auto leave") },
		{ ESteamJoinInMatchPolicy::AskGame,   true,  false, true,  ESteamJoinAction::Ignore,       TEXT("same session, ask game") },
		{ ESteamJoinInMatchPolicy::AskGame,   false, true,  true,  ESteamJoinAction::Ignore,       TEXT("same session, not in match") },
	};

	for (const FCase& Case : Cases)
	{
		TestTrue(Case.Name, DecideJoin(Case.Policy, Case.bInMatch, Case.bAutoJoin, Case.bAlreadyInTarget) == Case.Expected);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionShapeTest, "SandwichSteam.Sessions.Rules.Shape",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionShapeTest::RunTest(const FString& Parameters)
{
	const FSessionShape Public = MakeShape(ESteamSessionVisibility::Public, 4, true, true);
	TestEqual(TEXT("Public slots"), Public.PublicConnections, 4);
	TestEqual(TEXT("Public has no private slots"), Public.PrivateConnections, 0);
	TestTrue(TEXT("Public is advertised"), Public.bShouldAdvertise);
	TestTrue(TEXT("Public can be joined through presence"), Public.bAllowJoinViaPresence && !Public.bAllowJoinViaPresenceFriendsOnly);
	TestTrue(TEXT("Presence sessions are lobbies"), Public.bUseLobbies && Public.bUsesPresence);

	const FSessionShape Friends = MakeShape(ESteamSessionVisibility::FriendsOnly, 8, true, false);
	TestFalse(TEXT("Friends only is not advertised"), Friends.bShouldAdvertise);
	TestTrue(TEXT("Friends only joins through presence, friends only"), Friends.bAllowJoinViaPresence && Friends.bAllowJoinViaPresenceFriendsOnly);
	TestFalse(TEXT("Join in progress off"), Friends.bAllowJoinInProgress);

	const FSessionShape Private = MakeShape(ESteamSessionVisibility::Private, 2, true, true);
	TestEqual(TEXT("Private slots"), Private.PrivateConnections, 2);
	TestEqual(TEXT("Private has no public slots"), Private.PublicConnections, 0);
	TestFalse(TEXT("Private is not joinable through presence"), Private.bAllowJoinViaPresence);
	TestTrue(TEXT("Private still allows invites"), Private.bAllowInvites);

	const FSessionShape NoPresence = MakeShape(ESteamSessionVisibility::Public, 4, false, true);
	TestFalse(TEXT("No presence means no lobby"), NoPresence.bUseLobbies);
	TestFalse(TEXT("No presence means no join through presence"), NoPresence.bAllowJoinViaPresence);

	// The search only lists lobbies, so presence and lobbies must always travel together (Phase 14c).
	for (const ESteamSessionVisibility Visibility : { ESteamSessionVisibility::Public, ESteamSessionVisibility::FriendsOnly, ESteamSessionVisibility::Private })
	{
		for (const bool bPresence : { true, false })
		{
			const FSessionShape Shape = MakeShape(Visibility, 4, bPresence, true);
			TestEqual(TEXT("Presence and lobbies are paired"), Shape.bUseLobbies, Shape.bUsesPresence);
			TestEqual(TEXT("Presence comes from the request"), Shape.bUsesPresence, bPresence);
		}
	}

	TestEqual(TEXT("Slots are clamped low"), MakeShape(ESteamSessionVisibility::Public, 0, true, true).PublicConnections, 1);
	TestEqual(TEXT("Slots are clamped high"), MakeShape(ESteamSessionVisibility::Public, 9999, true, true).PublicConnections, 250);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionFailureTest, "SandwichSteam.Sessions.Rules.NetworkFailure",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionFailureTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Connection lost is HostLeft"), ClassifyNetworkFailure(ENetworkFailure::ConnectionLost) == ELostReason::HostLeft);
	TestTrue(TEXT("Timeout is Timeout"), ClassifyNetworkFailure(ENetworkFailure::ConnectionTimeout) == ELostReason::Timeout);
	TestTrue(TEXT("Failure received is Kicked"), ClassifyNetworkFailure(ENetworkFailure::FailureReceived) == ELostReason::Kicked);
	TestTrue(TEXT("Outdated client is Failed"), ClassifyNetworkFailure(ENetworkFailure::OutdatedClient) == ELostReason::Failed);
	TestTrue(TEXT("Driver failure is Failed"), ClassifyNetworkFailure(ENetworkFailure::NetDriverCreateFailure) == ELostReason::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionDuplicateTest, "SandwichSteam.Sessions.Rules.Duplicate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionDuplicateTest::RunTest(const FString& Parameters)
{
	const FSteamId A(109775241234567890);
	const FSteamId B(109775241234567891);

	TestTrue(TEXT("Same lobby inside the window"), IsDuplicateRequest(105.0, 100.0, A, A, 10.0f));
	TestFalse(TEXT("Same lobby after the window"), IsDuplicateRequest(120.0, 100.0, A, A, 10.0f));
	TestFalse(TEXT("Another lobby"), IsDuplicateRequest(101.0, 100.0, A, B, 10.0f));
	TestFalse(TEXT("Window 0 disables the check"), IsDuplicateRequest(100.0, 100.0, A, A, 0.0f));
	TestFalse(TEXT("Invalid lobby is never a duplicate"), IsDuplicateRequest(100.0, 100.0, FSteamId(), FSteamId(), 10.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionConnectStringTest, "SandwichSteam.Sessions.Rules.ConnectString",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionConnectStringTest::RunTest(const FString& Parameters)
{
	FSteamJoinIntent Intent;

	TestTrue(TEXT("Lobby form parses"), ParseConnectString(TEXT("+connect_lobby 109775241234567890"), Intent));
	TestTrue(TEXT("Lobby type"), Intent.Type == ESteamJoinIntentType::Lobby);
	TestEqual(TEXT("Lobby id"), Intent.LobbyId.Value, static_cast<int64>(109775241234567890));

	TestTrue(TEXT("Lobby form with other words"), ParseConnectString(TEXT("-windowed +connect_lobby 109775241234567890 -log"), Intent));
	TestTrue(TEXT("Still a lobby"), Intent.Type == ESteamJoinIntentType::Lobby);

	TestTrue(TEXT("Server form parses"), ParseConnectString(TEXT("+connect 192.168.0.5:7777"), Intent));
	TestTrue(TEXT("Server type"), Intent.Type == ESteamJoinIntentType::Server);
	TestEqual(TEXT("Server address"), Intent.ServerAddress, FString(TEXT("192.168.0.5:7777")));

	TestFalse(TEXT("Empty text"), ParseConnectString(FString(), Intent));
	TestFalse(TEXT("Other text"), ParseConnectString(TEXT("hello world"), Intent));
	TestFalse(TEXT("Lobby without an id"), ParseConnectString(TEXT("+connect_lobby"), Intent));
	TestFalse(TEXT("Lobby with a bad id"), ParseConnectString(TEXT("+connect_lobby nonsense"), Intent));
	TestTrue(TEXT("A failed parse leaves no intent"), Intent.Type == ESteamJoinIntentType::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamLobbyKeysTest, "SandwichSteam.Sessions.Rules.LobbyKeys",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamLobbyKeysTest::RunTest(const FString& Parameters)
{
	FText Error;

	TestTrue(TEXT("A normal key"), ValidateLobbyKey(TEXT("map"), Error));
	TestFalse(TEXT("Empty key"), ValidateLobbyKey(FString(), Error));
	TestFalse(TEXT("Key over 255 bytes"), ValidateLobbyKey(FString::ChrN(256, TEXT('k')), Error));
	TestTrue(TEXT("Key of exactly 255 bytes"), ValidateLobbyKey(FString::ChrN(255, TEXT('k')), Error));
	TestFalse(TEXT("Profile key is reserved"), ValidateLobbyKey(ProfileKey(), Error));
	TestFalse(TEXT("Kick marker is reserved"), ValidateLobbyKey(TEXT("kick_76561197960287930"), Error));
	TestFalse(TEXT("Kick prefix is reserved in any case"), ValidateLobbyKey(TEXT("KICK_x"), Error));
	TestTrue(TEXT("A key that only contains 'kick'"), ValidateLobbyKey(TEXT("nokick_"), Error));

	TestTrue(TEXT("Empty value is allowed"), ValidateLobbyValue(FString(), Error));
	TestFalse(TEXT("Value over 8192 bytes"), ValidateLobbyValue(FString::ChrN(8193, TEXT('v')), Error));
	// A two byte character counts as two bytes.
	TestFalse(TEXT("Bytes, not characters"), ValidateLobbyValue(FString::ChrN(4100, static_cast<TCHAR>(0x00E9)), Error));

	TestFalse(TEXT("Empty chat"), ValidateChatText(FString(), Error));
	TestTrue(TEXT("Normal chat"), ValidateChatText(TEXT("hello"), Error));
	TestFalse(TEXT("Chat over 4096 bytes"), ValidateChatText(FString::ChrN(4097, TEXT('c')), Error));

	TestEqual(TEXT("Kick key"), KickKey(FSteamId(76561197960287930)), FString(TEXT("kick_76561197960287930")));
	TestTrue(TEXT("Flag 1"), ParseFlag(TEXT("1")));
	TestTrue(TEXT("Flag true"), ParseFlag(TEXT("True")));
	TestFalse(TEXT("Flag 0"), ParseFlag(TEXT("0")));
	TestFalse(TEXT("Flag empty"), ParseFlag(FString()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamLobbyMemberChangeTest, "SandwichSteam.Sessions.Rules.LobbyMemberChange",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamLobbyMemberChangeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Entered"), MemberChangeFromFlags(0x1) == ESteamLobbyMemberChange::Entered);
	TestTrue(TEXT("Left"), MemberChangeFromFlags(0x2) == ESteamLobbyMemberChange::Left);
	TestTrue(TEXT("Disconnected"), MemberChangeFromFlags(0x4) == ESteamLobbyMemberChange::Disconnected);
	TestTrue(TEXT("Kicked"), MemberChangeFromFlags(0x8) == ESteamLobbyMemberChange::Kicked);
	TestTrue(TEXT("Banned"), MemberChangeFromFlags(0x10) == ESteamLobbyMemberChange::Banned);
	TestTrue(TEXT("Strongest flag wins"), MemberChangeFromFlags(0x2 | 0x8) == ESteamLobbyMemberChange::Kicked);

	TestTrue(TEXT("Enter success"), ClassifyLobbyEnter(1) == ELobbyEnterOutcome::Success);
	TestTrue(TEXT("Enter does not exist"), ClassifyLobbyEnter(2) == ELobbyEnterOutcome::NotFound);
	TestTrue(TEXT("Enter full"), ClassifyLobbyEnter(4) == ELobbyEnterOutcome::Full);
	TestTrue(TEXT("Enter not allowed"), ClassifyLobbyEnter(3) == ELobbyEnterOutcome::Denied);
	TestTrue(TEXT("Enter banned"), ClassifyLobbyEnter(6) == ELobbyEnterOutcome::Denied);
	TestTrue(TEXT("Enter blocked"), ClassifyLobbyEnter(10) == ELobbyEnterOutcome::Denied);
	TestTrue(TEXT("Enter error"), ClassifyLobbyEnter(5) == ELobbyEnterOutcome::Failed);
	TestTrue(TEXT("Enter unknown code"), ClassifyLobbyEnter(99) == ELobbyEnterOutcome::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionResolveSettingsTest, "SandwichSteam.Sessions.Rules.ResolveSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionResolveSettingsTest::RunTest(const FString& Parameters)
{
	FSteamSessionSettings Applied;
	FText Error;

	// No profile: only plugin limits.
	{
		FSteamSessionSettings Requested;
		Requested.MaxPlayers = 0;
		TestTrue(TEXT("No profile resolves"), ResolveSessionSettings(nullptr, Requested, Applied, Error));
		TestEqual(TEXT("No profile default players"), Applied.MaxPlayers, DefaultSessionPlayers);

		Requested.MaxPlayers = 9999;
		TestTrue(TEXT("No profile clamps high"), ResolveSessionSettings(nullptr, Requested, Applied, Error));
		TestEqual(TEXT("No profile clamped high"), Applied.MaxPlayers, MaxSessionPlayers);

		Requested.MaxPlayers = -5;
		TestTrue(TEXT("No profile clamps low (falls back to default)"), ResolveSessionSettings(nullptr, Requested, Applied, Error));
		TestEqual(TEXT("Negative falls back to default"), Applied.MaxPlayers, DefaultSessionPlayers);

		Requested.MaxPlayers = 4;
		Requested.Settings.Add(TEXT("OSTPROFILE"), TEXT("x"));
		TestFalse(TEXT("No profile refuses reserved keys"), ResolveSessionSettings(nullptr, Requested, Applied, Error));
	}

	FSteamSessionProfileDef Profile;
	Profile.Tag = FGameplayTag();
	Profile.MinPlayers = 2;
	Profile.MaxPlayers = 8;
	Profile.DefaultPlayers = 4;
	Profile.Visibility = ESteamSessionVisibility::Public;
	Profile.AllowedVisibilities = (1 << static_cast<uint8>(ESteamSessionVisibility::Public)) | (1 << static_cast<uint8>(ESteamSessionVisibility::FriendsOnly));
	Profile.bAllowJoinInProgress = true;
	Profile.bPlayerCanChangeJoinInProgress = false;
	Profile.bUsesPresence = true;
	Profile.Settings.Add(TEXT("Fixed"), TEXT("1"));
	Profile.PlayerSettings.Add(TEXT("Map"), TEXT("Default"));
	Profile.bAllowExtraSettings = false;

	// Default players.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		TestTrue(TEXT("Profile default players resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestEqual(TEXT("Profile default players"), Applied.MaxPlayers, 4);
		TestEqual(TEXT("Fixed setting present"), Applied.Settings.FindRef(TEXT("Fixed")), FString(TEXT("1")));
		TestEqual(TEXT("Player setting default present"), Applied.Settings.FindRef(TEXT("Map")), FString(TEXT("Default")));
	}

	// Clamp low / high against Min/Max.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.MaxPlayers = 1;
		TestTrue(TEXT("Clamp low resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestEqual(TEXT("Clamped to Min Players"), Applied.MaxPlayers, 2);

		Requested.MaxPlayers = 999;
		TestTrue(TEXT("Clamp high resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestEqual(TEXT("Clamped to Max Players"), Applied.MaxPlayers, 8);
	}

	// Allowed / refused visibility.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::FriendsOnly;
		Requested.bAllowJoinInProgress = true;
		TestTrue(TEXT("Allowed visibility resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestTrue(TEXT("Applied visibility"), Applied.Visibility == ESteamSessionVisibility::FriendsOnly);

		Requested.Visibility = ESteamSessionVisibility::Private;
		TestFalse(TEXT("Refused visibility fails"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestFalse(TEXT("Refused visibility sets an error"), Error.IsEmpty());
	}

	// Join in progress locked.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = false; // Profile default is true, and the player cannot change it.
		TestFalse(TEXT("Locked join in progress refused"), ResolveSessionSettings(&Profile, Requested, Applied, Error));

		Requested.bAllowJoinInProgress = true; // Matches the locked default: allowed.
		TestTrue(TEXT("Matching locked value resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
	}

	// Fixed key refused.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.Settings.Add(TEXT("Fixed"), TEXT("2"));
		TestFalse(TEXT("Overriding a fixed setting is refused"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
	}

	// Player key default and override, extra keys off.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.Settings.Add(TEXT("Map"), TEXT("Arena"));
		TestTrue(TEXT("Overriding a player setting resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestEqual(TEXT("Player setting overridden"), Applied.Settings.FindRef(TEXT("Map")), FString(TEXT("Arena")));

		Requested.Settings.Reset();
		Requested.Settings.Add(TEXT("Extra"), TEXT("1"));
		TestFalse(TEXT("Extra key refused when Allow Extra Settings is off"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
	}

	// Extra keys on.
	{
		FSteamSessionProfileDef Open = Profile;
		Open.bAllowExtraSettings = true;
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.Settings.Add(TEXT("Extra"), TEXT("1"));
		TestTrue(TEXT("Extra key accepted when Allow Extra Settings is on"), ResolveSessionSettings(&Open, Requested, Applied, Error));
		TestEqual(TEXT("Extra key present"), Applied.Settings.FindRef(TEXT("Extra")), FString(TEXT("1")));
	}

	// Reserved keys, even with a profile.
	{
		FSteamSessionProfileDef Open = Profile;
		Open.bAllowExtraSettings = true;
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.Settings.Add(TEXT("kick_1"), TEXT("1"));
		TestFalse(TEXT("Reserved key refused with a profile"), ResolveSessionSettings(&Open, Requested, Applied, Error));
	}

	// Name trim and cap.
	{
		FSteamSessionProfileDef Named = Profile;
		Named.MaxNameLength = 5;
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.DisplayName = TEXT("  Hello World  ");
		TestTrue(TEXT("Name resolves"), ResolveSessionSettings(&Named, Requested, Applied, Error));
		TestEqual(TEXT("Name trimmed and capped"), Applied.DisplayName, FString(TEXT("Hello")));
	}

	// bUsesPresence is designer only: the request is ignored.
	{
		FSteamSessionSettings Requested;
		Requested.Visibility = ESteamSessionVisibility::Public;
		Requested.bAllowJoinInProgress = true;
		Requested.bUsesPresence = false;
		TestTrue(TEXT("Presence resolves"), ResolveSessionSettings(&Profile, Requested, Applied, Error));
		TestTrue(TEXT("Presence comes from the profile, not the request"), Applied.bUsesPresence == Profile.bUsesPresence);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionDefaultSettingsTest, "SandwichSteam.Sessions.Rules.DefaultSettings",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionDefaultSettingsTest::RunTest(const FString& Parameters)
{
	FSteamSessionProfileDef Profile;
	Profile.MaxPlayers = 8;
	Profile.DefaultPlayers = 0; // 0 means Max Players.
	Profile.Visibility = ESteamSessionVisibility::FriendsOnly;
	Profile.bAllowJoinInProgress = false;
	Profile.Settings.Add(TEXT("Fixed"), TEXT("1"));
	Profile.PlayerSettings.Add(TEXT("Map"), TEXT("Default"));

	FSteamSessionSettings Defaults;
	MakeDefaultSettings(Profile, Defaults);
	TestEqual(TEXT("Default players is Max Players"), Defaults.MaxPlayers, 8);
	TestTrue(TEXT("Default visibility"), Defaults.Visibility == ESteamSessionVisibility::FriendsOnly);
	TestFalse(TEXT("Default join in progress"), Defaults.bAllowJoinInProgress);
	TestEqual(TEXT("Default fixed setting"), Defaults.Settings.FindRef(TEXT("Fixed")), FString(TEXT("1")));
	TestEqual(TEXT("Default player setting"), Defaults.Settings.FindRef(TEXT("Map")), FString(TEXT("Default")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSessionUpdateValidationTest, "SandwichSteam.Sessions.Rules.UpdateValidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamSessionUpdateValidationTest::RunTest(const FString& Parameters)
{
	FSteamSessionSettings Current;
	Current.MaxPlayers = 4;
	Current.Visibility = ESteamSessionVisibility::Public;
	Current.bAllowJoinInProgress = true;
	Current.bUsesPresence = true;

	FSteamSessionSettings Applied;
	FText Error;

	FSteamSessionSettings Requested = Current;
	Requested.MaxPlayers = 2;
	TestFalse(TEXT("Max Players below current players is refused"), ValidateSessionUpdate(nullptr, Current, Requested, /*CurrentPlayers*/ 3, Applied, Error));

	Requested.MaxPlayers = 6;
	TestTrue(TEXT("Max Players at or above current players resolves"), ValidateSessionUpdate(nullptr, Current, Requested, /*CurrentPlayers*/ 3, Applied, Error));

	Requested.MaxPlayers = 6;
	Requested.bUsesPresence = false;
	TestFalse(TEXT("Changing Uses Presence is refused"), ValidateSessionUpdate(nullptr, Current, Requested, /*CurrentPlayers*/ 3, Applied, Error));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
