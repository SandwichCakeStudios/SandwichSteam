// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/SteamPublishSettings.h"

namespace SandwichSteam::Publish
{
	struct FVdfFiles
	{
		FString AppVdfPath;
		TArray<FString> DepotVdfPaths;
		/** SteamCMD logs and cache. */
		FString BuildOutputDir;
		/** Non fatal problems (missing content folder, ...). */
		TArray<FString> Warnings;
	};

	/** Scripts, SteamCMD output and history. Setting "Publish Directory", else <Data Directory>/Publish (default Saved/SandwichSteam/Publish). */
	FString GetPublishDir();

	/** Setting "Staging Directory", else <Data Directory>/StagedBuilds (default Saved/SandwichSteam/StagedBuilds), absolute. */
	FString GetStagingDir();

	/** Staged folder name UAT uses for the platform (Windows, Mac, Linux). */
	FString GetStagedFolderName(ESteamPublishPlatform Platform);

	/** Absolute, forward slash content root of the depot. Relative paths start at the project folder; empty = <Staging Directory>/<Platform>. */
	FString ResolveContentRoot(const FSteamPublishDepot& Depot);

	/** The build description with its tokens ({Project}, {Config}, {Branch}, {Date}) filled in, as the app script gets it. */
	FString FormatBuildDescription(const USteamPublishSettings& Settings, const FString& BranchName);

	/** Writes the app and depot scripts for the enabled depots. Returns false with OutError when the settings cannot produce a valid build. */
	bool WriteVdfFiles(const USteamPublishSettings& Settings, const FString& BranchName, FVdfFiles& OutFiles, FString& OutError);
}
