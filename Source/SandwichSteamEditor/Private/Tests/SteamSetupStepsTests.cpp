// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Setup/SteamSetupSteps.h"

namespace
{
	ESteamSetupStepState StateOf(const TArray<FSteamSetupStep>& Steps, ESteamSetupStepId Id)
	{
		return Steps[static_cast<int32>(Id)].State;
	}

	FSteamSetupInputs MakeCompleteInputs()
	{
		FSteamSetupInputs In;
		In.AppId = 1234560;
		In.bDefinitionAssigned = true;
		In.bDefinitionLoads = true;
		In.bDefinitionCooked = true;
		In.bSteamCmdFound = true;
		In.bUsernameValid = true;
		In.LoginState = ESteamSetupLoginState::Ok;
		In.EnabledDepotCount = 1;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSetupStepsEmptyTest, "SandwichSteam.Editor.Setup.EmptyProject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamSetupStepsEmptyTest::RunTest(const FString& Parameters)
{
	const TArray<FSteamSetupStep> Steps = SandwichSteam::Editor::EvaluateSetupSteps(FSteamSetupInputs());

	TestEqual(TEXT("One entry per step"), Steps.Num(), static_cast<int32>(ESteamSetupStepId::Count));
	TestTrue(TEXT("App ID to do"), StateOf(Steps, ESteamSetupStepId::AppId) == ESteamSetupStepState::Todo);
	TestTrue(TEXT("Definition to do"), StateOf(Steps, ESteamSetupStepId::AppDefinition) == ESteamSetupStepState::Todo);
	TestTrue(TEXT("SteamCMD to do"), StateOf(Steps, ESteamSetupStepId::SteamCmd) == ESteamSetupStepState::Todo);
	TestTrue(TEXT("Account waits for SteamCMD"), StateOf(Steps, ESteamSetupStepId::Account) == ESteamSetupStepState::Blocked);
	TestTrue(TEXT("Depots wait"), StateOf(Steps, ESteamSetupStepId::Depots) == ESteamSetupStepState::Blocked);
	TestFalse(TEXT("Not complete"), SandwichSteam::Editor::IsSetupComplete(Steps));
	TestEqual(TEXT("Next is the App ID"), SandwichSteam::Editor::FindNextSetupStep(Steps), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSetupStepsCompleteTest, "SandwichSteam.Editor.Setup.Complete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamSetupStepsCompleteTest::RunTest(const FString& Parameters)
{
	const TArray<FSteamSetupStep> Steps = SandwichSteam::Editor::EvaluateSetupSteps(MakeCompleteInputs());
	TestTrue(TEXT("Complete"), SandwichSteam::Editor::IsSetupComplete(Steps));
	TestEqual(TEXT("No next step"), SandwichSteam::Editor::FindNextSetupStep(Steps), static_cast<int32>(INDEX_NONE));

	// Depots that already exist count the account as done without a login check this session.
	FSteamSetupInputs Unchecked = MakeCompleteInputs();
	Unchecked.LoginState = ESteamSetupLoginState::Unknown;
	TestTrue(TEXT("Complete without a login check"), SandwichSteam::Editor::IsSetupComplete(SandwichSteam::Editor::EvaluateSetupSteps(Unchecked)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamSetupStepsRulesTest, "SandwichSteam.Editor.Setup.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamSetupStepsRulesTest::RunTest(const FString& Parameters)
{
	using namespace SandwichSteam::Editor;

	{
		FSteamSetupInputs In = MakeCompleteInputs();
		In.AppId = SpacewarAppId;
		In.EnabledDepotCount = 0;
		const TArray<FSteamSetupStep> Steps = EvaluateSetupSteps(In);
		TestTrue(TEXT("Spacewar is not a publishable App ID"), StateOf(Steps, ESteamSetupStepId::AppId) == ESteamSetupStepState::Todo);
		TestTrue(TEXT("Depots wait for a real App ID"), StateOf(Steps, ESteamSetupStepId::Depots) == ESteamSetupStepState::Blocked);
	}
	{
		FSteamSetupInputs In = MakeCompleteInputs();
		In.bDefinitionCooked = false;
		TestTrue(TEXT("Uncooked definition"), StateOf(EvaluateSetupSteps(In), ESteamSetupStepId::AppDefinition) == ESteamSetupStepState::Todo);
	}
	{
		FSteamSetupInputs In = MakeCompleteInputs();
		In.bSteamCmdFound = false;
		In.bSteamCmdDownloading = true;
		const TArray<FSteamSetupStep> Steps = EvaluateSetupSteps(In);
		TestTrue(TEXT("Download runs"), StateOf(Steps, ESteamSetupStepId::SteamCmd) == ESteamSetupStepState::Running);
		TestTrue(TEXT("Account waits for SteamCMD"), StateOf(Steps, ESteamSetupStepId::Account) == ESteamSetupStepState::Blocked);
	}
	{
		FSteamSetupInputs In = MakeCompleteInputs();
		In.EnabledDepotCount = 0;
		In.LoginState = ESteamSetupLoginState::NeedsLogin;
		const TArray<FSteamSetupStep> Steps = EvaluateSetupSteps(In);
		TestTrue(TEXT("Login needed"), StateOf(Steps, ESteamSetupStepId::Account) == ESteamSetupStepState::Todo);
		TestTrue(TEXT("Depots wait for the login"), StateOf(Steps, ESteamSetupStepId::Depots) == ESteamSetupStepState::Blocked);
		TestEqual(TEXT("Next is the account"), FindNextSetupStep(Steps), static_cast<int32>(ESteamSetupStepId::Account));
	}
	{
		FSteamSetupInputs In = MakeCompleteInputs();
		In.EnabledDepotCount = 0;
		TestTrue(TEXT("Ready to fetch"), StateOf(EvaluateSetupSteps(In), ESteamSetupStepId::Depots) == ESteamSetupStepState::Todo);
		In.bFetchingAppInfo = true;
		TestTrue(TEXT("Fetch runs"), StateOf(EvaluateSetupSteps(In), ESteamSetupStepId::Depots) == ESteamSetupStepState::Running);
	}
	{
		FSteamSetupInputs In = MakeCompleteInputs();
		In.EnabledDepotCount = 0;
		In.LoginState = ESteamSetupLoginState::Checking;
		TestTrue(TEXT("Login check runs"), StateOf(EvaluateSetupSteps(In), ESteamSetupStepId::Account) == ESteamSetupStepState::Running);
	}
	return true;
}

#endif
