// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPackager.h"
#include "Misc/Paths.h"
#include "Publish/SteamVdfWriter.h"

FString FSteamPackager::GetPlatformName(ESteamPublishPlatform Platform)
{
	switch (Platform)
	{
	case ESteamPublishPlatform::Mac:
		return TEXT("Mac");
	case ESteamPublishPlatform::Linux:
		return TEXT("Linux");
	default:
		return TEXT("Win64");
	}
}

FString FSteamPackager::BuildUatArguments(const FSteamPackageRequest& Request)
{
	TArray<FString> PlatformNames;
	for (const ESteamPublishPlatform Platform : Request.Platforms)
	{
		PlatformNames.AddUnique(GetPlatformName(Platform));
	}

	const TCHAR* ConfigName = Request.Config == ESteamPublishConfig::Shipping ? TEXT("Shipping") : TEXT("Development");

	FString Args = FString::Printf(TEXT("BuildCookRun -project=\"%s\" -noP4 -utf8output -unattended -platform=%s -clientconfig=%s -build -cook -stage -pak -compressed"),
		*FSteamVdfWriter::NormalizePath(Request.ProjectFile), *FString::Join(PlatformNames, TEXT("+")), ConfigName);

	if (!Request.StagingDir.IsEmpty())
	{
		Args += FString::Printf(TEXT(" -stagingdirectory=\"%s\""), *FSteamVdfWriter::NormalizePath(Request.StagingDir));
	}
	if (!Request.TargetName.IsEmpty())
	{
		Args += FString::Printf(TEXT(" -target=\"%s\""), *Request.TargetName.TrimStartAndEnd());
	}
	return Args;
}

void FSteamPackager::BuildUatCommand(const FSteamPackageRequest& Request, FString& OutExecutable, FString& OutArguments)
{
	const FString BatchDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build") / TEXT("BatchFiles"));
	const FString Args = BuildUatArguments(Request);
#if PLATFORM_WINDOWS
	FString Script = BatchDir / TEXT("RunUAT.bat");
	Script.ReplaceInline(TEXT("/"), TEXT("\\"));
	// cmd removes the outer quotes of a /c string that starts with a quote, so the script path keeps its own.
	OutExecutable = TEXT("cmd.exe");
	OutArguments = FString::Printf(TEXT("/c \"\"%s\" %s\""), *Script, *Args);
#else
	OutExecutable = TEXT("/bin/bash");
	OutArguments = FString::Printf(TEXT("\"%s\" %s"), *(BatchDir / TEXT("RunUAT.sh")), *Args);
#endif
}

void FSteamPackager::BuildStepCommand(const FSteamPublishStep& Step, const FString& DefaultWorkingDir, FString& OutExecutable, FString& OutArguments)
{
	FString WorkingDir = Step.WorkingDirectory.TrimStartAndEnd();
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = DefaultWorkingDir;
	}
	else if (FPaths::IsRelative(WorkingDir))
	{
		WorkingDir = FPaths::Combine(DefaultWorkingDir, WorkingDir);
	}
	WorkingDir = FPaths::ConvertRelativePathToFull(WorkingDir);

#if PLATFORM_WINDOWS
	WorkingDir.ReplaceInline(TEXT("/"), TEXT("\\"));
	OutExecutable = TEXT("cmd.exe");
	OutArguments = FString::Printf(TEXT("/c \"cd /d \"%s\" && \"%s\" %s\""), *WorkingDir, *Step.Executable, *Step.Arguments);
#else
	OutExecutable = TEXT("/bin/sh");
	OutArguments = FString::Printf(TEXT("-c \"cd \\\"%s\\\" && \\\"%s\\\" %s\""), *WorkingDir, *Step.Executable, *Step.Arguments);
#endif
}

ESteamLogSeverity FSteamPackager::ClassifyLine(const FString& Line)
{
	if (Line.Contains(TEXT("Warning:")) || Line.Contains(TEXT("warning C")) || Line.Contains(TEXT("WARNING")))
	{
		return ESteamLogSeverity::Warning;
	}
	if (Line.Contains(TEXT("Error:")) || Line.Contains(TEXT("ERROR")) || Line.Contains(TEXT("error C")) || Line.Contains(TEXT("BUILD FAILED")) || Line.Contains(TEXT("Failed")))
	{
		return ESteamLogSeverity::Error;
	}
	return ESteamLogSeverity::Info;
}
