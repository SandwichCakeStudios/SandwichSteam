// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Presence/SteamPresenceVdfWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPresenceVdfPlaceholdersTest, "SandwichSteam.Editor.PresenceVdf.Placeholders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamPresenceVdfPlaceholdersTest::RunTest(const FString& Parameters)
{
	const TSet<FString> Known = { TEXT("map"), TEXT("score") };

	TArray<FString> Unknown;
	TestEqual(TEXT("Keys become %key%"), FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("Playing on {map} - {score} points"), Known, &Unknown), FString(TEXT("Playing on %map% - %score% points")));
	TestEqual(TEXT("Known keys are not reported"), Unknown.Num(), 0);

	TestEqual(TEXT("Spaces inside braces are trimmed"), FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("{ map }"), Known), FString(TEXT("%map%")));

	FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("{map} {mode} {mode}"), Known, &Unknown);
	TestEqual(TEXT("Unknown keys are reported once"), Unknown.Num(), 1);
	TestEqual(TEXT("It is the mode"), Unknown.Num() == 1 ? Unknown[0] : FString(), FString(TEXT("mode")));

	TestEqual(TEXT("A backtick escapes the brace"), FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("Use `{map} here"), Known), FString(TEXT("Use {map} here")));
	TestEqual(TEXT("An unclosed brace stays"), FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("Broken {map"), Known), FString(TEXT("Broken {map")));
	TestEqual(TEXT("Empty braces stay"), FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("a {} b"), Known), FString(TEXT("a {} b")));
	TestEqual(TEXT("Plain text is unchanged"), FSteamPresenceVdfWriter::ConvertPlaceholders(TEXT("Main menu"), Known), FString(TEXT("Main menu")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPresenceVdfGoldenTest, "SandwichSteam.Editor.PresenceVdf.Golden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamPresenceVdfGoldenTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Nothing to write gives an empty file"), FSteamPresenceVdfWriter::Build(TArray<FSteamPresenceLanguage>()).IsEmpty());

	FSteamPresenceLanguage English;
	English.Language = TEXT("english");
	English.Tokens.Emplace(TEXT("#Status_Menu"), TEXT("In the main menu"));
	English.Tokens.Emplace(TEXT("#Status_InMatch"), TEXT("Playing on %map% - \"ranked\""));

	FSteamPresenceLanguage German;
	German.Language = TEXT("german");
	German.Tokens.Emplace(TEXT("#Status_Menu"), TEXT("Im Hauptmenü"));

	FSteamPresenceLanguage Empty;
	Empty.Language = TEXT("french");

	const FString Expected =
		TEXT("\"lang\"\n")
		TEXT("{\n")
		TEXT("\t\"english\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"tokens\"\n")
		TEXT("\t\t{\n")
		TEXT("\t\t\t\"#Status_Menu\"\t\"In the main menu\"\n")
		TEXT("\t\t\t\"#Status_InMatch\"\t\"Playing on %map% - \\\"ranked\\\"\"\n")
		TEXT("\t\t}\n")
		TEXT("\t}\n")
		TEXT("\t\"german\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"tokens\"\n")
		TEXT("\t\t{\n")
		TEXT("\t\t\t\"#Status_Menu\"\t\"Im Hauptmenü\"\n")
		TEXT("\t\t}\n")
		TEXT("\t}\n")
		TEXT("}\n");

	TArray<FSteamPresenceLanguage> Languages;
	Languages.Add(English);
	Languages.Add(Empty);
	Languages.Add(German);
	TestEqual(TEXT("File content (a language without tokens is skipped, quotes are escaped)"), FSteamPresenceVdfWriter::Build(Languages), Expected);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
