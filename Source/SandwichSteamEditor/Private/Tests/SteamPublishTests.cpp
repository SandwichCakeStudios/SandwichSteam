// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Publish/SteamPackager.h"
#include "Publish/SteamPublishHistory.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPackagerArgsTest, "SandwichSteam.Editor.Publish.PackagerArgs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamPackagerArgsTest::RunTest(const FString& Parameters)
{
	FSteamPackageRequest Request;
	Request.ProjectFile = TEXT("D:\\Proj\\My Game.uproject");
	Request.Platforms = { ESteamPublishPlatform::Win64, ESteamPublishPlatform::Linux, ESteamPublishPlatform::Win64 };
	Request.Config = ESteamPublishConfig::Shipping;
	Request.StagingDir = TEXT("D:\\Out\\Staged");

	const FString Args = FSteamPackager::BuildUatArguments(Request);
	TestTrue(TEXT("BuildCookRun"), Args.StartsWith(TEXT("BuildCookRun ")));
	TestTrue(TEXT("Project is quoted and uses forward slashes"), Args.Contains(TEXT("-project=\"D:/Proj/My Game.uproject\"")));
	TestTrue(TEXT("Platforms are joined without duplicates"), Args.Contains(TEXT("-platform=Win64+Linux ")));
	TestTrue(TEXT("Shipping config"), Args.Contains(TEXT("-clientconfig=Shipping")));
	TestTrue(TEXT("Staging directory"), Args.Contains(TEXT("-stagingdirectory=\"D:/Out/Staged\"")));
	TestTrue(TEXT("Cook, stage and pak"), Args.Contains(TEXT("-cook")) && Args.Contains(TEXT("-stage")) && Args.Contains(TEXT("-pak")));

	Request.Config = ESteamPublishConfig::Development;
	Request.StagingDir.Reset();
	const FString Dev = FSteamPackager::BuildUatArguments(Request);
	TestTrue(TEXT("Development config"), Dev.Contains(TEXT("-clientconfig=Development")));
	TestFalse(TEXT("No staging directory when empty"), Dev.Contains(TEXT("-stagingdirectory")));

	FString Executable, Arguments;
	FSteamPackager::BuildUatCommand(Request, Executable, Arguments);
	TestFalse(TEXT("UAT command has an executable"), Executable.IsEmpty());
	TestTrue(TEXT("UAT command runs BuildCookRun"), Arguments.Contains(TEXT("BuildCookRun")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPackagerLineTest, "SandwichSteam.Editor.Publish.LogSeverity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamPackagerLineTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Error line"), FSteamPackager::ClassifyLine(TEXT("LogInit: Error: something broke")) == ESteamLogSeverity::Error);
	TestTrue(TEXT("Warning line"), FSteamPackager::ClassifyLine(TEXT("LogCook: Warning: missing")) == ESteamLogSeverity::Warning);
	TestTrue(TEXT("Plain line"), FSteamPackager::ClassifyLine(TEXT("LogCook: Display: Cooked 10 packages")) == ESteamLogSeverity::Info);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamPublishHistoryTest, "SandwichSteam.Editor.Publish.History",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamPublishHistoryTest::RunTest(const FString& Parameters)
{
	FSteamPublishHistoryEntry A;
	A.Time = FDateTime(2026, 9, 25, 12, 30, 0);
	A.AppId = 1234;
	A.Branch = TEXT("beta");
	A.BuildId = 9876543210;
	A.bSuccess = true;
	A.Message = TEXT("Build uploaded \"ok\"");

	FSteamPublishHistoryEntry B;
	B.Time = FDateTime(2026, 9, 24, 8, 0, 0);
	B.AppId = 1234;
	B.bSuccess = false;
	B.Message = TEXT("Login failed");

	const FString Json = FSteamPublishHistory::ToJson({ A, B });

	TArray<FSteamPublishHistoryEntry> Read;
	TestTrue(TEXT("Parses"), FSteamPublishHistory::FromJson(Json, Read));
	TestEqual(TEXT("Two entries"), Read.Num(), 2);
	if (Read.Num() == 2)
	{
		TestEqual(TEXT("Time"), Read[0].Time, A.Time);
		TestEqual(TEXT("Branch"), Read[0].Branch, A.Branch);
		TestEqual(TEXT("Large BuildID survives"), Read[0].BuildId, A.BuildId);
		TestTrue(TEXT("Success"), Read[0].bSuccess);
		TestEqual(TEXT("Message with quotes"), Read[0].Message, A.Message);
		TestFalse(TEXT("Failure"), Read[1].bSuccess);
		TestEqual(TEXT("No BuildID"), Read[1].BuildId, static_cast<int64>(0));
	}

	TestFalse(TEXT("Garbage is rejected"), FSteamPublishHistory::FromJson(TEXT("nope"), Read));
	TestTrue(TEXT("Empty list is valid"), FSteamPublishHistory::FromJson(FSteamPublishHistory::ToJson({}), Read) && Read.IsEmpty());
	return true;
}

#endif
