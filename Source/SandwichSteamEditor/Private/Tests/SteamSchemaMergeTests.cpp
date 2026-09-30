// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Import/SteamSchemaMerge.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSchemaParseTest, "SandwichSteam.Editor.SchemaMerge.Parse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamSchemaParseTest::RunTest(const FString& Parameters)
{
	FSteamSchemaData Schema;
	FString Error;

	TestFalse(TEXT("Garbage is rejected"), FSteamSchemaMerge::ParseSchema(TEXT("not json"), Schema, Error));
	TestFalse(TEXT("Missing achievements array is rejected"), FSteamSchemaMerge::ParseSchema(TEXT("{\"appId\":480}"), Schema, Error));

	const FString Json = TEXT("{\"appId\":480,\"achievements\":["
		"{\"apiName\":\"ACH_A\",\"displayName\":\"A\",\"hidden\":true},"
		"{\"apiName\":\"ACH_B\"},"
		"{\"apiName\":\"ACH_A\",\"displayName\":\"duplicate\"},"
		"{\"displayName\":\"no name\"}"
		"],\"stats\":[]}");

	TestTrue(TEXT("Valid export parses"), FSteamSchemaMerge::ParseSchema(Json, Schema, Error));
	TestEqual(TEXT("App id"), Schema.AppId, 480);
	TestEqual(TEXT("Duplicates and nameless rows are skipped"), Schema.Achievements.Num(), 2);
	if (Schema.Achievements.Num() == 2)
	{
		TestEqual(TEXT("First row wins"), Schema.Achievements[0].DisplayName, FString(TEXT("A")));
		TestTrue(TEXT("Hidden flag"), Schema.Achievements[0].bHidden);
		TestFalse(TEXT("Hidden defaults to false"), Schema.Achievements[1].bHidden);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSchemaTagNameTest, "SandwichSteam.Editor.SchemaMerge.TagName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamSchemaTagNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Plain name"), FSteamSchemaMerge::MakeAchievementTagName(TEXT("ACH_WIN_ONE_GAME")), FString(TEXT("Steam.Achievement.ACH_WIN_ONE_GAME")));
	TestEqual(TEXT("Spaces, dots and dashes become underscores"), FSteamSchemaMerge::MakeAchievementTagName(TEXT("first win.v2-beta")), FString(TEXT("Steam.Achievement.first_win_v2_beta")));
	TestTrue(TEXT("None gives an empty name"), FSteamSchemaMerge::MakeAchievementTagName(NAME_None).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSchemaMergeRulesTest, "SandwichSteam.Editor.SchemaMerge.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamSchemaMergeRulesTest::RunTest(const FString& Parameters)
{
	FSteamSchemaData Schema;
	Schema.Achievements.Add({ TEXT("ACH_A"), TEXT("Alpha"), TEXT(""), true });
	Schema.Achievements.Add({ TEXT("ACH_B"), TEXT("Beta"), TEXT(""), false });
	Schema.Achievements.Add({ TEXT("ACH_C"), TEXT("Gamma"), TEXT(""), false });

	TArray<FSteamAchievementDef> Rows;
	{
		FSteamAchievementDef Existing;
		Existing.ApiName = TEXT("ACH_A");
		Existing.ProgressStat = TEXT("Kills");
		Existing.ProgressMax = 50;
		Existing.bHidden = false; // Steam says hidden: changes.
		Rows.Add(Existing);

		FSteamAchievementDef Removed;
		Removed.ApiName = TEXT("ACH_OLD");
		Rows.Add(Removed);

		FSteamAchievementDef Same;
		Same.ApiName = TEXT("ACH_B");
		Same.EditorDisplayName = TEXT("Beta");
		Rows.Add(Same);
	}

	int32 TagCalls = 0;
	const FSteamSchemaMergeResult Result = FSteamSchemaMerge::MergeAchievements(Rows, Schema, [&TagCalls](FName) { ++TagCalls; return FGameplayTag(); });

	TestEqual(TEXT("One row added"), Result.Added.Num(), 1);
	TestTrue(TEXT("ACH_C added"), Result.Added.Contains(FName(TEXT("ACH_C"))));
	TestEqual(TEXT("One row updated"), Result.Updated, 1);
	TestEqual(TEXT("One row unchanged"), Result.Unchanged, 1);
	TestEqual(TEXT("Nothing is deleted"), Rows.Num(), 4);
	TestEqual(TEXT("Missing row reported"), Result.MissingInSteam.Num(), 1);
	TestTrue(TEXT("ACH_OLD reported"), Result.MissingInSteam.Contains(FName(TEXT("ACH_OLD"))));
	TestEqual(TEXT("Tag callback runs for every untagged Steam row"), TagCalls, 3);
	TestEqual(TEXT("No tag was valid, none assigned"), Result.TagsAssigned, 0);

	// Existing rows keep their own settings.
	TestEqual(TEXT("Progress stat kept"), Rows[0].ProgressStat, FName(TEXT("Kills")));
	TestEqual(TEXT("Progress max kept"), Rows[0].ProgressMax, 50);
	TestTrue(TEXT("Hidden follows Steam"), Rows[0].bHidden);

	// Merging the same export again changes nothing (idempotent).
	const FSteamSchemaMergeResult Again = FSteamSchemaMerge::MergeAchievements(Rows, Schema, TFunction<FGameplayTag(FName)>());
	TestEqual(TEXT("Second merge adds nothing"), Again.Added.Num(), 0);
	TestEqual(TEXT("Second merge updates nothing"), Again.Updated, 0);
	TestEqual(TEXT("Second merge keeps the row count"), Rows.Num(), 4);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
