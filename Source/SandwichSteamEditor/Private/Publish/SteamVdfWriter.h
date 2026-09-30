// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSteamVdfFileMapping
{
	FString LocalPath = TEXT("*");
	FString DepotPath = TEXT(".");
	bool bRecursive = true;
};

struct FSteamVdfDepot
{
	int32 DepotId = 0;
	FString ContentRoot;
	TArray<FSteamVdfFileMapping> FileMappings;
	TArray<FString> FileExclusions;
};

struct FSteamVdfApp
{
	int32 AppId = 0;
	FString Description;
	/** Folder SteamCMD writes logs and its cache to. */
	FString BuildOutput;
	/** Optional. Depot scripts carry their own content root. */
	FString ContentRoot;
	/** Empty or "default" = the build is uploaded but not set live. */
	FString SetLiveBranch;
	/** Steam preview build: nothing is uploaded, Steam only reports what would change. */
	bool bPreview = false;
	TArray<int32> DepotIds;
};

/**
 * Pure text generation of the SteamPipe build scripts (app_build_<appid>.vdf and depot_build_<depot>.vdf).
 * No file access, no engine state, so it is unit tested with golden strings. Output uses tabs and LF line endings.
 */
class FSteamVdfWriter
{
public:
	/** Steam does not accept setlive on the default branch (Steam policy, [verify] with a real upload). */
	static bool CanSetLive(const FString& BranchName);

	/** File name of the depot script inside the app script folder. */
	static FString GetDepotFileName(int32 DepotId);
	static FString GetAppFileName(int32 AppId);

	/** Backslashes become forward slashes. */
	static FString NormalizePath(const FString& Path);

	/** Escapes backslash and quote and wraps the text in quotes. */
	static FString Quote(const FString& Text);

	static FString BuildApp(const FSteamVdfApp& App);
	static FString BuildDepot(const FSteamVdfDepot& Depot);

	/** Replaces {Token} occurrences. Unknown tokens stay untouched. */
	static FString FormatDescription(const FString& Template, const TMap<FString, FString>& Tokens);
};
