// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Setup/SteamSetupSteps.h"
#include "Cook/SteamCookHelper.h"
#include "Core/SteamToolSettings.h"
#include "HAL/FileManager.h"
#include "Publish/SteamCmdOutputParser.h"
#include "Publish/SteamCmdSetupService.h"
#include "Publish/SteamPublishSettings.h"

#define LOCTEXT_NAMESPACE "SandwichSteamSetupSteps"

namespace
{
	FSteamSetupStep MakeStep(ESteamSetupStepId Id, ESteamSetupStepState State, const FText& Detail)
	{
		FSteamSetupStep Step;
		Step.Id = Id;
		Step.State = State;
		Step.Detail = Detail;
		return Step;
	}

	FSteamSetupStep EvaluateAppId(const FSteamSetupInputs& In)
	{
		if (In.AppId <= 0)
		{
			return MakeStep(ESteamSetupStepId::AppId, ESteamSetupStepState::Todo,
				LOCTEXT("AppIdMissing", "Enter your game's App ID. It is the number in your app's URL on the Steamworks partner site."));
		}
		if (In.AppId == SandwichSteam::Editor::SpacewarAppId)
		{
			return MakeStep(ESteamSetupStepId::AppId, ESteamSetupStepState::Todo,
				LOCTEXT("AppIdSpacewar", "480 is Valve's Spacewar test app, which you cannot publish to. Enter your own App ID from the Steamworks partner site."));
		}
		return MakeStep(ESteamSetupStepId::AppId, ESteamSetupStepState::Done,
			FText::Format(LOCTEXT("AppIdOk", "App {0}."), FText::AsNumber(In.AppId, &FNumberFormattingOptions::DefaultNoGrouping())));
	}

	FSteamSetupStep EvaluateDefinition(const FSteamSetupInputs& In)
	{
		if (!In.bDefinitionAssigned)
		{
			return MakeStep(ESteamSetupStepId::AppDefinition, ESteamSetupStepState::Todo,
				LOCTEXT("DefinitionMissing", "None yet. One asset holds your stats, achievements, leaderboards, presence, DLC, sessions and input sets."));
		}
		if (!In.bDefinitionLoads)
		{
			return MakeStep(ESteamSetupStepId::AppDefinition, ESteamSetupStepState::Todo,
				LOCTEXT("DefinitionBroken", "The assigned asset could not be loaded. Create a new one."));
		}
		if (!In.bDefinitionCooked)
		{
			return MakeStep(ESteamSetupStepId::AppDefinition, ESteamSetupStepState::Todo,
				LOCTEXT("DefinitionNotCooked", "Assigned, but not part of the cook: packaged builds would not find it."));
		}
		return MakeStep(ESteamSetupStepId::AppDefinition, ESteamSetupStepState::Done, LOCTEXT("DefinitionOk", "Assigned and cooked."));
	}

	FSteamSetupStep EvaluateSteamCmd(const FSteamSetupInputs& In)
	{
		if (In.bSteamCmdDownloading)
		{
			return MakeStep(ESteamSetupStepId::SteamCmd, ESteamSetupStepState::Running, LOCTEXT("SteamCmdDownloading", "Installing SteamCMD..."));
		}
		if (!In.bSteamCmdFound)
		{
			return MakeStep(ESteamSetupStepId::SteamCmd, ESteamSetupStepState::Todo,
				LOCTEXT("SteamCmdMissing", "Not installed. SteamCMD is Valve's command line tool that uploads your builds."));
		}
		return MakeStep(ESteamSetupStepId::SteamCmd, ESteamSetupStepState::Done, LOCTEXT("SteamCmdOk", "Installed."));
	}

	FSteamSetupStep EvaluateAccount(const FSteamSetupInputs& In)
	{
		if (!In.bSteamCmdFound)
		{
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Blocked, LOCTEXT("AccountBlocked", "Needs SteamCMD first."));
		}
		if (In.LoginState == ESteamSetupLoginState::Checking)
		{
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Running, LOCTEXT("AccountChecking", "Checking the login..."));
		}
		if (!In.bUsernameValid)
		{
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Todo,
				LOCTEXT("AccountNoUser", "Enter the Steam account that uploads builds (it needs the Edit App Metadata and Publish App Changes permissions)."));
		}

		switch (In.LoginState)
		{
		case ESteamSetupLoginState::Ok:
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Done, LOCTEXT("AccountOk", "Logged in. SteamCMD remembers the login."));
		case ESteamSetupLoginState::NeedsLogin:
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Todo,
				LOCTEXT("AccountNeedsLogin", "Log in once: a terminal opens where you type your password and Steam Guard code. The login is checked again when you come back to the editor."));
		case ESteamSetupLoginState::Failed:
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Todo,
				LOCTEXT("AccountFailed", "The login check failed. If the login terminal is still open, type quit there, then press Verify."));
		default:
			break;
		}

		// Not checked this session. Depots that are already set up mean the login was working before; Publish checks it again anyway.
		if (In.EnabledDepotCount > 0)
		{
			return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Done, LOCTEXT("AccountAssumed", "Account set. The login is checked when you publish."));
		}
		return MakeStep(ESteamSetupStepId::Account, ESteamSetupStepState::Todo, LOCTEXT("AccountUnknown", "Account set. Log in once, or press Verify if you already have."));
	}

	FSteamSetupStep EvaluateDepots(const FSteamSetupInputs& In, const FSteamSetupStep& AppIdStep, const FSteamSetupStep& AccountStep)
	{
		if (In.EnabledDepotCount > 0)
		{
			return MakeStep(ESteamSetupStepId::Depots, ESteamSetupStepState::Done,
				FText::Format(LOCTEXT("DepotsOk", "{0} enabled depot(s)."), FText::AsNumber(In.EnabledDepotCount)));
		}
		if (In.bFetchingAppInfo)
		{
			return MakeStep(ESteamSetupStepId::Depots, ESteamSetupStepState::Running, LOCTEXT("DepotsFetching", "Reading depots and branches from Steam..."));
		}
		if (AppIdStep.State != ESteamSetupStepState::Done)
		{
			return MakeStep(ESteamSetupStepId::Depots, ESteamSetupStepState::Blocked, LOCTEXT("DepotsNeedAppId", "Needs your App ID first."));
		}
		if (AccountStep.State != ESteamSetupStepState::Done)
		{
			return MakeStep(ESteamSetupStepId::Depots, ESteamSetupStepState::Blocked, LOCTEXT("DepotsNeedLogin", "Needs a working Steam login first."));
		}
		return MakeStep(ESteamSetupStepId::Depots, ESteamSetupStepState::Todo,
			LOCTEXT("DepotsMissing", "No depots yet. Fetch them from Steam: the depots and branches of your app are added to the Publish settings."));
	}
}

namespace SandwichSteam::Editor
{
	TArray<FSteamSetupStep> EvaluateSetupSteps(const FSteamSetupInputs& Inputs)
	{
		TArray<FSteamSetupStep> Steps;
		Steps.Reserve(static_cast<int32>(ESteamSetupStepId::Count));
		Steps.Add(EvaluateAppId(Inputs));
		Steps.Add(EvaluateDefinition(Inputs));
		Steps.Add(EvaluateSteamCmd(Inputs));
		Steps.Add(EvaluateAccount(Inputs));
		Steps.Add(EvaluateDepots(Inputs, Steps[0], Steps[3]));
		return Steps;
	}

	bool IsSetupComplete(const TArray<FSteamSetupStep>& Steps)
	{
		return FindNextSetupStep(Steps) == INDEX_NONE;
	}

	int32 FindNextSetupStep(const TArray<FSteamSetupStep>& Steps)
	{
		return Steps.IndexOfByPredicate([](const FSteamSetupStep& Step) { return Step.State != ESteamSetupStepState::Done; });
	}

	FSteamSetupInputs GatherSetupInputs()
	{
		FSteamSetupInputs Inputs;

		if (const USteamToolSettings* Settings = USteamToolSettings::Get())
		{
			Inputs.AppId = Settings->SteamAppId;
			Inputs.bDefinitionAssigned = !Settings->AppDefinition.IsNull();
			Inputs.bDefinitionLoads = Inputs.bDefinitionAssigned && Settings->LoadAppDefinition() != nullptr;
			Inputs.bDefinitionCooked = Inputs.bDefinitionLoads && FSteamCookHelper::IsAppDefinitionCooked();
		}

		const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
		Inputs.bSteamCmdFound = !User->SteamCmdPath.FilePath.IsEmpty() && IFileManager::Get().FileExists(*User->SteamCmdPath.FilePath);
		Inputs.bUsernameValid = FSteamCmdCommandLine::IsValidUsername(User->SteamUsername.TrimStartAndEnd());

		for (const FSteamPublishDepot& Depot : USteamPublishSettings::Get()->Depots)
		{
			Inputs.EnabledDepotCount += (Depot.bEnabled && Depot.DepotId > 0) ? 1 : 0;
		}

		const FSteamCmdSetupService& Service = FSteamCmdSetupService::Get();
		Inputs.bSteamCmdDownloading = Service.IsDownloading();
		Inputs.LoginState = Service.GetLoginState();
		Inputs.bFetchingAppInfo = Service.IsFetchingAppInfo();
		return Inputs;
	}
}

#undef LOCTEXT_NAMESPACE
