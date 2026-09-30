// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Validation/SteamProjectValidator.h"

struct FSteamPublishOptions
{
	/** Branch to upload to. Empty = upload without setlive. */
	FString Branch;
	/** Write the build scripts only. */
	bool bDryRun = false;
	/** Upload the folder that is already staged instead of packaging first. */
	bool bSkipPackaging = false;
	/** Run pre steps, package, write the build scripts and post steps, but never call SteamCMD. Lets a developer verify packaging before spending an upload. */
	bool bSkipUpload = false;
	/** Switch Live Coding off while UAT packages and restore it afterwards. Turn off to manage Live Coding by hand. */
	bool bDisableLiveCoding = true;
};

/**
 * Checks that must pass before a publish starts. Errors stop the run, warnings are shown and the run continues.
 * Reuses FSteamValidationCheck so the panel draws them like the settings status strip.
 */
class FSteamPublishPreflight
{
public:
	static TArray<FSteamValidationCheck> Run(const FSteamPublishOptions& Options);

	static bool HasErrors(const TArray<FSteamValidationCheck>& Checks);
};
