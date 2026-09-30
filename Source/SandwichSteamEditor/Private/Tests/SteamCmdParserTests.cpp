// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Publish/SteamCmdOutputParser.h"

namespace
{
	int32 CountType(const TArray<FSteamCmdEvent>& Events, ESteamCmdEventType Type)
	{
		int32 Count = 0;
		for (const FSteamCmdEvent& Event : Events)
		{
			Count += Event.Type == Type ? 1 : 0;
		}
		return Count;
	}

	const FSteamCmdEvent* FindType(const TArray<FSteamCmdEvent>& Events, ESteamCmdEventType Type)
	{
		return Events.FindByPredicate([Type](const FSteamCmdEvent& Event) { return Event.Type == Type; });
	}
}

// The samples are written from known SteamCMD output, not captured from a real upload. Add real lines here when a
// [verify] run shows a different wording (see PROGRESS.md).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCmdParserLoginTest, "SandwichSteam.Editor.SteamCmd.Login",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamCmdParserLoginTest::RunTest(const FString& Parameters)
{
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("Redirecting stderr to 'logs/stderr.txt'\r\nLogging in user 'builder' [U:1:123] to Steam Public...OK\r\n"), Events);
		TestEqual(TEXT("Both lines are reported"), CountType(Events, ESteamCmdEventType::Line), 2);
		TestEqual(TEXT("Login ok"), CountType(Events, ESteamCmdEventType::LoginOk), 1);
		TestEqual(TEXT("No error"), CountType(Events, ESteamCmdEventType::Error), 0);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("Logging in user 'builder' to Steam Public...FAILED (Invalid Password)\n"), Events);
		const FSteamCmdEvent* Failed = FindType(Events, ESteamCmdEventType::LoginFailed);
		TestNotNull(TEXT("Login failed event"), Failed);
		if (Failed)
		{
			TestEqual(TEXT("Reason"), Failed->Text, FString(TEXT("Invalid Password")));
		}
		TestEqual(TEXT("Not reported as a generic error"), CountType(Events, ESteamCmdEventType::Error), 0);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("FAILED login with result code Rate Limit Exceeded\n"), Events);
		TestEqual(TEXT("Result code failure"), CountType(Events, ESteamCmdEventType::LoginFailed), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCmdParserPromptTest, "SandwichSteam.Editor.SteamCmd.Prompts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamCmdParserPromptTest::RunTest(const FString& Parameters)
{
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("This computer has not been authenticated for your account using Steam Guard.\r\nSteam Guard code:"), Events);
		TestEqual(TEXT("E-mail prompt without newline"), CountType(Events, ESteamCmdEventType::GuardEmailRequested), 1);
		Parser.Feed(TEXT("\r\nLogging in user 'builder' to Steam Public...OK\r\n"), Events);
		TestEqual(TEXT("Prompt is reported once"), CountType(Events, ESteamCmdEventType::GuardEmailRequested), 1);
		TestEqual(TEXT("Login after the code"), CountType(Events, ESteamCmdEventType::LoginOk), 1);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("Two-fac"), Events);
		TestEqual(TEXT("Split prompt is not reported early"), Events.Num(), 0);
		Parser.Feed(TEXT("tor code:"), Events);
		TestEqual(TEXT("Mobile prompt"), CountType(Events, ESteamCmdEventType::GuardMobileRequested), 1);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("password:"), Events);
		TestEqual(TEXT("Password prompt"), CountType(Events, ESteamCmdEventType::PasswordRequested), 1);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("FAILED (No cached credentials and @NoPromptForPassword is set)\n"), Events);
		TestEqual(TEXT("NoPromptForPassword failure means a password is needed"), CountType(Events, ESteamCmdEventType::PasswordRequested), 1);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("\"@NoPromptForPassword\" = \"1\"\n"), Events);
		TestEqual(TEXT("Echo of the setting is not a password request"), CountType(Events, ESteamCmdEventType::PasswordRequested), 0);
	}
	{
		FSteamCmdOutputParser Parser;
		TArray<FSteamCmdEvent> Events;
		Parser.Feed(TEXT("\"@ShutdownOnFailedCommand\" = \"1\"\n"), Events);
		TestEqual(TEXT("Echo of ShutdownOnFailedCommand is not an error"), CountType(Events, ESteamCmdEventType::Error), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCmdParserBuildTest, "SandwichSteam.Editor.SteamCmd.Build",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamCmdParserBuildTest::RunTest(const FString& Parameters)
{
	FSteamCmdOutputParser Parser;
	TArray<FSteamCmdEvent> Events;
	Parser.Feed(TEXT("Checking for available updates...\r\nDownloading update (12,000 of 30,000 KB)...\r\n"), Events);
	TestEqual(TEXT("Self update lines"), CountType(Events, ESteamCmdEventType::SelfUpdate), 2);

	Events.Reset();
	Parser.Feed(TEXT("Update state (0x61) downloading, progress: 45.50 (100 / 220)\r"), Events);
	const FSteamCmdEvent* Progress = FindType(Events, ESteamCmdEventType::Progress);
	TestNotNull(TEXT("Progress line"), Progress);
	if (Progress)
	{
		TestTrue(TEXT("Percent"), FMath::IsNearlyEqual(Progress->Value, 45.5f, 0.01f));
	}

	Events.Reset();
	Parser.Feed(TEXT("ERROR! Failed to commit build for AppID 480, (Failure)\nSuccessfully finished AppID 480 build (BuildID 12345678)\n"), Events);
	TestEqual(TEXT("Error line"), CountType(Events, ESteamCmdEventType::Error), 1);
	const FSteamCmdEvent* Finished = FindType(Events, ESteamCmdEventType::BuildFinished);
	TestNotNull(TEXT("Build finished"), Finished);
	if (Finished)
	{
		TestEqual(TEXT("BuildID"), Finished->Number, static_cast<int64>(12345678));
	}

	Events.Reset();
	Parser.Feed(TEXT("last line without newline"), Events);
	TestEqual(TEXT("Unfinished line waits"), Events.Num(), 0);
	Parser.Flush(Events);
	TestEqual(TEXT("Flush reports it"), CountType(Events, ESteamCmdEventType::Line), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCmdCommandLineTest, "SandwichSteam.Editor.SteamCmd.CommandLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamCmdCommandLineTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Plain user"), FSteamCmdCommandLine::IsValidUsername(TEXT("build_bot-1.x@mail")));
	TestFalse(TEXT("Empty"), FSteamCmdCommandLine::IsValidUsername(FString()));
	TestFalse(TEXT("Space would start a new argument"), FSteamCmdCommandLine::IsValidUsername(TEXT("a +quit")));
	TestFalse(TEXT("Quote"), FSteamCmdCommandLine::IsValidUsername(TEXT("a\"b")));

	const FString Upload = FSteamCmdCommandLine::BuildUpload(TEXT("bot"), TEXT("D:/P/app_build_480.vdf"));
	TestTrue(TEXT("Upload runs the build script"), Upload.Contains(TEXT("+run_app_build \"D:/P/app_build_480.vdf\"")));
	TestTrue(TEXT("Upload never prompts for a password"), Upload.Contains(TEXT("+@NoPromptForPassword 1")));
	TestTrue(TEXT("Terminal login may prompt"), !FSteamCmdCommandLine::BuildTerminalLogin(TEXT("bot")).Contains(TEXT("NoPromptForPassword")));
	return true;
}

#endif
