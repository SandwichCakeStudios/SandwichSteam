// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ESteamCmdEventType : uint8
{
	/** Any complete output line (Text). Always sent first for a line. */
	Line,
	/** SteamCMD updates itself. Uploading has not started. */
	SelfUpdate,
	LoginOk,
	/** Text = reason (for example "Invalid Password"). */
	LoginFailed,
	/** SteamCMD wants a password. This tool never sends one: the login has to be cached first. */
	PasswordRequested,
	/** Steam Guard code sent by e-mail is expected. */
	GuardEmailRequested,
	/** Steam Guard mobile authenticator code is expected. */
	GuardMobileRequested,
	/** Value = percent 0..100. */
	Progress,
	/** Number = BuildID. */
	BuildFinished,
	/** Text = the line. */
	Error
};

struct FSteamCmdEvent
{
	ESteamCmdEventType Type = ESteamCmdEventType::Line;
	FString Text;
	int64 Number = 0;
	float Value = 0.f;
};

/**
 * Turns raw SteamCMD output into events. Pure (no process, no engine state), unit tested.
 * Chunks may split a line anywhere. \r and \n both end a line (SteamCMD redraws progress with \r). Prompts that never end with a
 * newline (Steam Guard, password) are recognised in the unfinished line.
 * The patterns come from known SteamCMD output and are [verify] items: check them against a real upload and add lines to the tests.
 */
class FSteamCmdOutputParser
{
public:
	void Feed(const FString& Chunk, TArray<FSteamCmdEvent>& OutEvents);

	/** Process ended: whatever is left counts as a line. */
	void Flush(TArray<FSteamCmdEvent>& OutEvents);

private:
	void ParseLine(const FString& Line, TArray<FSteamCmdEvent>& OutEvents) const;
	bool ParsePrompt(const FString& Partial, TArray<FSteamCmdEvent>& OutEvents) const;

	FString Pending;
};

/** Builds SteamCMD command lines. The password is never part of any of them. */
struct FSteamCmdCommandLine
{
	/** Steam account names are letters, digits and _ . - @ only. Anything else could inject SteamCMD commands. */
	static bool IsValidUsername(const FString& Username);

	/** "+login <user> +quit": checks that the cached login works. */
	static FString BuildLoginCheck(const FString& Username);

	/** Uploads through the app build script. */
	static FString BuildUpload(const FString& Username, const FString& AppVdfPath);

	/** "+login <user> +app_info_update 1 +app_info_print <AppId> +quit": prints the app's KeyValues (depots, branches, ...). */
	static FString BuildAppInfoPrint(const FString& Username, int32 AppId);

	/** Interactive login for the terminal (password and Steam Guard are typed by the user there). */
	static FString BuildTerminalLogin(const FString& Username);
};
