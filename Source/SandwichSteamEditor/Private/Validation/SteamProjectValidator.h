// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Result level of one project check. */
enum class ESteamCheckSeverity : uint8
{
	Ok,
	/** Worth knowing, nothing to fix (for example App ID 480). */
	Info,
	Warning,
	/** Steam will not work until this is fixed. */
	Error
};

/** One line of the status strip and of the Message Log. */
struct FSteamValidationCheck
{
	FName Id;
	ESteamCheckSeverity Severity = ESteamCheckSeverity::Ok;

	/** Short name of what was checked, e.g. "Steam App ID". */
	FText Label;

	/** What was found and what to do about it. */
	FText Detail;

	/** Button text of the fix. Empty when the check has no automatic fix. */
	FText FixLabel;

	/** Runs the fix. Null when there is none. The caller re-runs the validator afterwards. */
	TFunction<void()> Fix;

	bool IsIssue() const { return Severity == ESteamCheckSeverity::Warning || Severity == ESteamCheckSeverity::Error; }
};

/**
 * Checks the project for everything Sandwich Steam needs and offers one-click fixes where that is possible.
 * Used by the settings status strip and by Tools > Sandwich Steam > Validate Project (Message Log category "Sandwich Steam").
 */
class FSteamProjectValidator
{
public:
	/** Runs every check. Reads config files and loads the App Definition, so call it on demand, not every frame. */
	static TArray<FSteamValidationCheck> Run();

	/** Writes the issues of Checks to the Message Log (a new page). Opens the log when bOpen and there is an issue. */
	static void WriteToMessageLog(const TArray<FSteamValidationCheck>& Checks, bool bOpen);

	/** Run() followed by WriteToMessageLog(). */
	static void RunAndLog(bool bOpen);

	/** Registers the "Sandwich Steam" Message Log listing. Call from StartupModule. */
	static void RegisterMessageLog();

	/** Creates /Game/Steam/DA_SteamAppDefinition (a unique name if it exists), saves it and assigns it in the settings. */
	static void CreateAndAssignAppDefinition();

	/** Message Log listing name. */
	static FName GetLogName();
};
