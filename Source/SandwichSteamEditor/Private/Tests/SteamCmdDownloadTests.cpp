// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Publish/SteamCmdDownloadJob.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCmdManifestJsonTest, "SandwichSteam.Editor.Publish.SteamCmdManifestJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamCmdManifestJsonTest::RunTest(const FString& Parameters)
{
	FSteamCmdManifest Manifest;
	Manifest.Platform = TEXT("Win64");
	Manifest.Url = TEXT("https://client-update.steamstatic.com/installer/steamcmd.zip");
	Manifest.DownloadedUtc = FDateTime(2026, 9, 27, 10, 0, 0);
	Manifest.InstalledPath = TEXT("D:\\Proj\\Saved\\SandwichSteam\\SteamCMD\\Win64\\steamcmd.exe");

	const FString Json = SandwichSteam::Editor::ManifestToJson(Manifest);

	FSteamCmdManifest Read;
	TestTrue(TEXT("Parses"), SandwichSteam::Editor::ManifestFromJson(Json, Read));
	TestEqual(TEXT("Platform"), Read.Platform, Manifest.Platform);
	TestEqual(TEXT("Url"), Read.Url, Manifest.Url);
	TestEqual(TEXT("DownloadedUtc"), Read.DownloadedUtc, Manifest.DownloadedUtc);
	TestEqual(TEXT("InstalledPath"), Read.InstalledPath, Manifest.InstalledPath);

	TestFalse(TEXT("Garbage is rejected"), SandwichSteam::Editor::ManifestFromJson(TEXT("nope"), Read));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamCmdShallowestPathTest, "SandwichSteam.Editor.Publish.SteamCmdShallowestPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamCmdShallowestPathTest::RunTest(const FString& Parameters)
{
	FString Best;
	TestFalse(TEXT("Empty list"), SandwichSteam::Editor::PickShallowestPath({}, Best));

	// A flat archive layout (steamcmd.exe at the install root) is preferred over one nested in a sub folder.
	const TArray<FString> Nested = {
		TEXT("D:/Install/linux32/steamcmd.exe"),
		TEXT("D:/Install/steamcmd.exe"),
		TEXT("D:/Install/some/deeper/copy/steamcmd.exe")
	};
	TestTrue(TEXT("Picks one"), SandwichSteam::Editor::PickShallowestPath(Nested, Best));
	TestEqual(TEXT("Shallowest wins"), Best, TEXT("D:/Install/steamcmd.exe"));

	// A single match, already nested, is still returned.
	const TArray<FString> OnlyNested = { TEXT("D:/Install/sub/steamcmd.exe") };
	TestTrue(TEXT("Single match"), SandwichSteam::Editor::PickShallowestPath(OnlyNested, Best));
	TestEqual(TEXT("Only candidate"), Best, TEXT("D:/Install/sub/steamcmd.exe"));
	return true;
}

#endif
