// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Publish/SteamPackager.h"

class FSteamProcessRunner;

/** What was downloaded and when, written next to the extracted files so the SteamCMD page can show it without re-downloading. */
struct FSteamCmdManifest
{
	/** Host editor platform this SteamCMD was downloaded for: "Win64", "Mac" or "Linux". */
	FString Platform;
	FString Url;
	FDateTime DownloadedUtc;
	/** Absolute path of the located steamcmd executable at the time of the download. */
	FString InstalledPath;
};

namespace SandwichSteam::Editor
{
	/** <Data Directory>/SteamCMD (Sandwich Steam settings). Holds the download cache, the per platform install folder and Manifest.json. */
	FString GetSteamCmdRoot();

	/** "Win64", "Mac" or "Linux" - the host editor platform, used for the install folder name and the manifest's Platform field. */
	FString GetHostPlatformFolderName();

	/** <SteamCMD Root>/<Win64|Mac|Linux>, where the archive is extracted for the host editor platform. */
	FString GetSteamCmdInstallDir();

	/** steamcmd.exe (Windows) or steamcmd.sh (Mac/Linux), the host editor platform's executable name. */
	FString GetSteamCmdExecutableName();

	/** Pure serialization, exposed for tests. */
	FString ManifestToJson(const FSteamCmdManifest& Manifest);
	bool ManifestFromJson(const FString& Json, FSteamCmdManifest& OutManifest);

	/** Reads <InstallRoot>/Manifest.json. Unset when it does not exist or does not parse. */
	TOptional<FSteamCmdManifest> ReadSteamCmdManifest(const FString& InstallRoot);

	/** Writes <InstallRoot>/Manifest.json, creating InstallRoot if needed. */
	bool WriteSteamCmdManifest(const FString& InstallRoot, const FSteamCmdManifest& Manifest);

	/**
	 * Pure selection: given every path already found under a folder (forward slash, relative or absolute, does not
	 * matter which as long as they are consistent), picks the one with the fewest path separators - a flat archive
	 * layout is preferred over a deeply nested one. False when Candidates is empty. Exposed for tests;
	 * FindSteamCmdExecutable does the real directory search and calls this.
	 */
	bool PickShallowestPath(const TArray<FString>& Candidates, FString& OutPath);

	/** Searches InstallDir (recursively) for the steamcmd executable and returns the shallowest match. Empty when not found. */
	FString FindSteamCmdExecutable(const FString& InstallDir);
}

/**
 * Downloads, extracts and self-updates SteamCMD for whichever editor platform is running (Windows, Mac or Linux).
 * One step at a time - download (Http), extract (FSteamProcessRunner + the OS's own tar), self-update
 * (FSteamProcessRunner, a bare "+quit" - never FSteamCmdRunner, since there is no login involved). Every delegate
 * fires on the game thread. Modeled on FSteamPublishJob, much smaller. Not started twice: create a new job.
 */
class FSteamCmdDownloadJob : public TSharedFromThis<FSteamCmdDownloadJob>
{
public:
	/** Progress is 0..1 across the whole job (download ~0-60%, extract ~60-85%, self-update ~85-100%), or negative while unknown. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProgress, float, const FText& /*StepLabel*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLog, const FString&, ESteamLogSeverity);
	/** InstalledExePath is only set when bSuccess is true. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnFinished, bool /*bSuccess*/, const FText& /*Message*/, const FString& /*InstalledExePath*/);

	FSteamCmdDownloadJob();
	~FSteamCmdDownloadJob();

	bool Start(FString& OutError);

	/** Stops the running download or extract/self-update process. OnFinished still fires. */
	void Cancel();

	bool IsRunning() const { return bRunning; }

	FOnProgress& OnProgress() { return ProgressDelegate; }
	FOnLog& OnLog() { return LogDelegate; }
	FOnFinished& OnFinished() { return FinishedDelegate; }

private:
	void Log(const FString& Line, ESteamLogSeverity Severity = ESteamLogSeverity::Info);
	void Finish(bool bSuccess, const FText& Message, const FString& ExePath = FString());

	void BeginDownload();
	void HandleDownloadProgress(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);
	void HandleDownloadComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	void BeginExtract();
	void HandleProcessLine(const FString& Line);
	void HandleExtractFinished(int32 ReturnCode, bool bCanceled);

	void BeginSelfUpdate();
	void HandleSelfUpdateFinished(int32 ReturnCode, bool bCanceled);

	void WriteManifestAndFinish();

	bool bRunning = false;
	bool bCancelRequested = false;

	FString DownloadUrl;
	FString DownloadedArchivePath;
	FString InstallDir;
	FString FoundExePath;

	FHttpRequestPtr DownloadRequest;
	TSharedPtr<FSteamProcessRunner> ProcessRunner;

	FOnProgress ProgressDelegate;
	FOnLog LogDelegate;
	FOnFinished FinishedDelegate;
};
