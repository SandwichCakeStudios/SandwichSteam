// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Publish/SteamVdfWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVdfAppGoldenTest, "SandwichSteam.Editor.Vdf.AppGolden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamVdfAppGoldenTest::RunTest(const FString& Parameters)
{
	FSteamVdfApp App;
	App.AppId = 480;
	App.Description = TEXT("Test 1.0");
	App.BuildOutput = TEXT("D:\\Proj\\Saved\\Out");
	App.SetLiveBranch = TEXT("beta");
	App.DepotIds = { 481, 482 };

	const FString Expected =
		TEXT("\"AppBuild\"\n")
		TEXT("{\n")
		TEXT("\t\"AppID\" \"480\"\n")
		TEXT("\t\"Desc\" \"Test 1.0\"\n")
		TEXT("\t\"Preview\" \"0\"\n")
		TEXT("\t\"BuildOutput\" \"D:/Proj/Saved/Out/\"\n")
		TEXT("\t\"SetLive\" \"beta\"\n")
		TEXT("\t\"Depots\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"481\" \"depot_build_481.vdf\"\n")
		TEXT("\t\t\"482\" \"depot_build_482.vdf\"\n")
		TEXT("\t}\n")
		TEXT("}\n");
	TestEqual(TEXT("App script"), FSteamVdfWriter::BuildApp(App), Expected);

	App.SetLiveBranch = TEXT("default");
	TestFalse(TEXT("default branch never gets SetLive"), FSteamVdfWriter::BuildApp(App).Contains(TEXT("SetLive")));
	App.SetLiveBranch = TEXT("Default");
	TestFalse(TEXT("default is case insensitive"), FSteamVdfWriter::BuildApp(App).Contains(TEXT("SetLive")));
	App.SetLiveBranch.Reset();
	TestFalse(TEXT("empty branch gives no SetLive"), FSteamVdfWriter::BuildApp(App).Contains(TEXT("SetLive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVdfDepotGoldenTest, "SandwichSteam.Editor.Vdf.DepotGolden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamVdfDepotGoldenTest::RunTest(const FString& Parameters)
{
	FSteamVdfDepot Depot;
	Depot.DepotId = 481;
	Depot.ContentRoot = TEXT("D:\\Proj\\Saved\\StagedBuilds\\Windows");
	Depot.FileMappings.Add(FSteamVdfFileMapping());
	Depot.FileExclusions = { TEXT("*.pdb"), TEXT("steam_appid.txt") };

	const FString Expected =
		TEXT("\"DepotBuildConfig\"\n")
		TEXT("{\n")
		TEXT("\t\"DepotID\" \"481\"\n")
		TEXT("\t\"ContentRoot\" \"D:/Proj/Saved/StagedBuilds/Windows\"\n")
		TEXT("\t\"FileMapping\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"LocalPath\" \"*\"\n")
		TEXT("\t\t\"DepotPath\" \".\"\n")
		TEXT("\t\t\"Recursive\" \"1\"\n")
		TEXT("\t}\n")
		TEXT("\t\"FileExclusion\" \"*.pdb\"\n")
		TEXT("\t\"FileExclusion\" \"steam_appid.txt\"\n")
		TEXT("}\n");
	TestEqual(TEXT("Depot script"), FSteamVdfWriter::BuildDepot(Depot), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVdfHelpersTest, "SandwichSteam.Editor.Vdf.Helpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamVdfHelpersTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Quote escapes quotes and backslashes"), FSteamVdfWriter::Quote(TEXT("a\"b\\c")), FString(TEXT("\"a\\\"b\\\\c\"")));

	TMap<FString, FString> Tokens;
	Tokens.Add(TEXT("Project"), TEXT("Game"));
	Tokens.Add(TEXT("Branch"), TEXT("beta"));
	TestEqual(TEXT("Tokens are replaced, unknown ones stay"), FSteamVdfWriter::FormatDescription(TEXT("{Project} on {Branch} {Other}"), Tokens), FString(TEXT("Game on beta {Other}")));

	TestEqual(TEXT("Depot file name"), FSteamVdfWriter::GetDepotFileName(7), FString(TEXT("depot_build_7.vdf")));
	TestEqual(TEXT("App file name"), FSteamVdfWriter::GetAppFileName(7), FString(TEXT("app_build_7.vdf")));
	return true;
}

#endif
