// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FSteamCmdRunner;
struct FSteamCmdEvent;
struct FSteamCmdResult;

/**
 * Runs "app_info_print <AppId>" through SteamCMD with the cached login (never a password) and writes the app's KeyValues block
 * (depots, branches, config) as JSON to <Publish Directory>/AppInfo/app_info_<AppId>.json. SteamCMD prints Valve KeyValues, not JSON,
 * so the text is converted (FSteamAppInfo::ParseKeyValues) before it is saved.
 * Game thread only. Steam Guard prompts are not answered: a login that needs one counts as expired.
 */
class FSteamAppInfoFetcher : public TSharedFromThis<FSteamAppInfoFetcher>
{
public:
	/** AppInfo is the parsed JSON (valid on success only). */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnFinished, bool /*bSuccess*/, const FText& /*Message*/, const FString& /*FilePath*/, const TSharedPtr<FJsonObject>& /*AppInfo*/);

	FSteamAppInfoFetcher();
	~FSteamAppInfoFetcher();

	/** Uses the SteamCMD path and account of the user settings and the App ID of the runtime settings. */
	bool Start(FString& OutError);

	/** OnFinished still fires (cancelled). */
	void Cancel();

	bool IsRunning() const;

	/** Once per Start. */
	FOnFinished& OnFinished() { return FinishedDelegate; }

	/** Absolute path the output of the given app is written to. */
	static FString GetOutputPath(int32 AppId);

	/** Cuts the app's top level block ("<AppId>" { ... }) out of the full SteamCMD output. Empty when it is not in there. Pure, unit testable. */
	static FString ExtractAppBlock(const TArray<FString>& Lines, int32 AppId);

private:
	void HandleEvent(const FSteamCmdEvent& Event);
	void HandleFinished(const FSteamCmdResult& Result);

	TSharedPtr<FSteamCmdRunner> Runner;
	FOnFinished FinishedDelegate;
	TArray<FString> Lines;
	int32 AppId = 0;
	bool bGuardRequested = false;
};
