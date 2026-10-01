// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Result of the last SteamCMD login check in this editor session. Never stored on disk. */
enum class ESteamSetupLoginState : uint8
{
	/** Not checked yet (or the account / SteamCMD path changed since). */
	Unknown,
	Checking,
	Ok,
	/** SteamCMD has no cached login: the one-time terminal login is needed. */
	NeedsLogin,
	/** Steam refused the login, or SteamCMD failed for another reason. */
	Failed
};

/** One row of the Setup page, in the order they are listed. */
enum class ESteamSetupStepId : uint8
{
	AppId,
	AppDefinition,
	SteamCmd,
	Account,
	Depots,

	Count
};

enum class ESteamSetupStepState : uint8
{
	Done,
	/** Something to do, and nothing stops the user from doing it now. */
	Todo,
	/** A job for this step is running. */
	Running,
	/** Waits for an earlier step. */
	Blocked
};

/** Everything the steps are decided from. Plain values, so EvaluateSetupSteps is pure and unit testable. */
struct FSteamSetupInputs
{
	int32 AppId = 0;

	bool bDefinitionAssigned = false;
	bool bDefinitionLoads = false;
	bool bDefinitionCooked = false;

	/** The SteamCMD path is set and the file exists. */
	bool bSteamCmdFound = false;
	bool bSteamCmdDownloading = false;

	bool bUsernameValid = false;
	ESteamSetupLoginState LoginState = ESteamSetupLoginState::Unknown;

	/** Depots in the Publish settings that are enabled and have a Depot ID. */
	int32 EnabledDepotCount = 0;
	bool bFetchingAppInfo = false;
};

struct FSteamSetupStep
{
	ESteamSetupStepId Id = ESteamSetupStepId::AppId;
	ESteamSetupStepState State = ESteamSetupStepState::Todo;

	/** What was found and, when not done, exactly what to do. */
	FText Detail;
};

namespace SandwichSteam::Editor
{
	/** Valve's Spacewar test app. Counts as "not set" here: it has no depots you can publish to. */
	constexpr int32 SpacewarAppId = 480;

	/** One entry per ESteamSetupStepId, in order. Pure. */
	TArray<FSteamSetupStep> EvaluateSetupSteps(const FSteamSetupInputs& Inputs);

	/** True when every step is Done. */
	bool IsSetupComplete(const TArray<FSteamSetupStep>& Steps);

	/** Index of the first step that is not Done, or INDEX_NONE. */
	int32 FindNextSetupStep(const TArray<FSteamSetupStep>& Steps);

	/** Reads the settings, the SteamCMD path and FSteamCmdSetupService. Loads the App Definition (small asset), so call it on change, not every frame. */
	FSteamSetupInputs GatherSetupInputs();
}
