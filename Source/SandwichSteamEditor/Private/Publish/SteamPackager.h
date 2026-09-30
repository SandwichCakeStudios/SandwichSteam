// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/SteamPublishSettings.h"

/** Log line level, derived from the text of a tool's output. */
enum class ESteamLogSeverity : uint8
{
	Info,
	Warning,
	Error
};

struct FSteamPackageRequest
{
	/** Absolute path of the .uproject. */
	FString ProjectFile;
	TArray<ESteamPublishPlatform> Platforms;
	ESteamPublishConfig Config = ESteamPublishConfig::Shipping;
	/** UAT stages into <StagingDir>/<Windows|Mac|Linux>. */
	FString StagingDir;
	/** -target=<Name>. Empty lets UAT pick the project's default Game target. */
	FString TargetName;
};

/**
 * Builds the Unreal Automation Tool (BuildCookRun) command line and the shell command lines of the pre and post steps.
 * Pure text, unit tested. The process itself runs through FSteamProcessRunner, so the panel gets one streamed log and a real cancel.
 * (IUATHelperModule was not used: it has its own notification and no access to the output.)
 */
class FSteamPackager
{
public:
	/** UAT platform name: Win64, Mac, Linux. */
	static FString GetPlatformName(ESteamPublishPlatform Platform);

	/** "BuildCookRun -project=... -platform=Win64+Linux ..." (without RunUAT). */
	static FString BuildUatArguments(const FSteamPackageRequest& Request);

	/** Executable and arguments that start RunUAT with BuildUatArguments (cmd.exe /c RunUAT.bat on Windows, bash RunUAT.sh elsewhere). */
	static void BuildUatCommand(const FSteamPackageRequest& Request, FString& OutExecutable, FString& OutArguments);

	/** Runs Step in its working directory through the platform shell. */
	static void BuildStepCommand(const FSteamPublishStep& Step, const FString& DefaultWorkingDir, FString& OutExecutable, FString& OutArguments);

	/** Error for lines with "error"/"Error:" (not "0 errors"), Warning for "Warning:". */
	static ESteamLogSeverity ClassifyLine(const FString& Line);
};
