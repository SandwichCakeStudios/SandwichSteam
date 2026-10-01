// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Setup/SteamSetupSteps.h"

class FJsonObject;
class FSteamAppInfoFetcher;
class FSteamCmdDownloadJob;
class FSteamCmdRunner;
struct FSteamCmdEvent;
struct FSteamCmdResult;

/** How a finished task treats its result. */
enum class ESteamCmdTaskMode : uint8
{
	/** Apply the result right away (SteamCMD path, fetched depots). Used by the Setup page. */
	Automatic,
	/** Leave applying to the caller, which asks the user first. Used by the SteamCMD page. */
	Confirm
};

/**
 * The one owner of the SteamCMD jobs the dashboard starts: the download, the login check and the app info fetch. Both the
 * Setup page and the SteamCMD page are built at the same time, so the jobs live here instead of in a page: one job of each kind
 * at a time, one notification per result, and both pages show the same progress.
 * Game thread only. Created on first use, destroyed (running jobs cancelled) by Shutdown from ShutdownModule.
 */
class FSteamCmdSetupService : public TSharedFromThis<FSteamCmdSetupService>
{
public:
	/** A job started or finished, or the login state changed. Not fired for download progress (read GetDownloadProgress). */
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	/** InstalledExePath is only set on success. bPathApplied: the SteamCMD path setting was already set (Automatic mode). */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnDownloadFinished, bool /*bSuccess*/, const FString& /*InstalledExePath*/, ESteamCmdTaskMode /*Mode*/, bool /*bPathApplied*/);
	/** AppInfo is valid on success only. In Automatic mode the depots and branches were already added. */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnAppInfoFinished, bool /*bSuccess*/, const FString& /*FilePath*/, const TSharedPtr<FJsonObject>& /*AppInfo*/, ESteamCmdTaskMode /*Mode*/);

	FSteamCmdSetupService();
	~FSteamCmdSetupService();

	static FSteamCmdSetupService& Get();

	/** Cancels running jobs and destroys the instance. Call from ShutdownModule. */
	static void Shutdown();

	FOnChanged& OnChanged() { return ChangedDelegate; }
	FOnDownloadFinished& OnDownloadFinished() { return DownloadFinishedDelegate; }
	FOnAppInfoFinished& OnAppInfoFinished() { return AppInfoFinishedDelegate; }

	//~ Download
	bool StartDownload(ESteamCmdTaskMode Mode, FString& OutError);
	void CancelDownload();
	bool IsDownloading() const;
	/** 0..1, or negative while unknown. */
	float GetDownloadProgress() const { return DownloadProgress; }
	const FText& GetDownloadStep() const { return DownloadStep; }

	//~ Login
	/** Opens the one-time login terminal and checks the login automatically when the editor is focused again. */
	void BeginTerminalLogin();
	/** "+login <user> +quit" with the cached login. bInteractive: a Steam Guard prompt opens a dialog (otherwise it counts as NeedsLogin) and every result is notified. */
	bool StartLoginCheck(bool bInteractive, FString& OutError);
	void CancelLoginCheck();
	bool IsCheckingLogin() const;
	/** Unknown when the account name or SteamCMD path changed since the last check. */
	ESteamSetupLoginState GetLoginState() const;

	//~ App info
	bool StartAppInfoFetch(ESteamCmdTaskMode Mode, FString& OutError);
	void CancelAppInfoFetch();
	bool IsFetchingAppInfo() const;
	/** One line about the last fetch (what was added, or why it failed). Empty before the first fetch. */
	const FText& GetLastAppInfoSummary() const { return LastAppInfoSummary; }

	bool IsAnyRunning() const;

	/** For a tab's CanClose: true when nothing runs; otherwise asks to cancel and cancels on Yes. */
	bool ConfirmCancelForClose();

	/**
	 * When the SteamCMD path is empty but an earlier download (Manifest.json) left a working executable, sets the path to it.
	 * Returns true when it set the path.
	 */
	bool AdoptDownloadedSteamCmd();

private:
	void HandleDownloadProgress(float Progress, const FText& StepLabel);
	void HandleDownloadFinished(bool bSuccess, const FText& Message, const FString& InstalledExePath);

	void HandleLoginEvent(const FSteamCmdEvent& Event);
	void HandleLoginFinished(const FSteamCmdResult& Result);
	void SetLoginState(ESteamSetupLoginState NewState);

	void HandleAppInfoFinished(bool bSuccess, const FText& Message, const FString& FilePath, const TSharedPtr<FJsonObject>& AppInfo);

	void HandleApplicationActivationChanged(bool bIsActive);

	TSharedPtr<FSteamCmdDownloadJob> DownloadJob;
	ESteamCmdTaskMode DownloadMode = ESteamCmdTaskMode::Confirm;
	float DownloadProgress = -1.f;
	FText DownloadStep;

	TSharedPtr<FSteamCmdRunner> LoginRunner;
	bool bLoginInteractive = false;
	bool bLoginGuardRequested = false;
	ESteamSetupLoginState LoginState = ESteamSetupLoginState::Unknown;
	/** Account and executable the LoginState belongs to. */
	FString CheckedUsername;
	FString CheckedExePath;
	/** Set by BeginTerminalLogin until a check succeeds. */
	bool bAwaitingTerminalLogin = false;
	double TerminalOpenedTime = 0.0;

	TSharedPtr<FSteamAppInfoFetcher> AppInfoFetcher;
	ESteamCmdTaskMode AppInfoMode = ESteamCmdTaskMode::Confirm;
	FText LastAppInfoSummary;

	FDelegateHandle ActivationHandle;

	FOnChanged ChangedDelegate;
	FOnDownloadFinished DownloadFinishedDelegate;
	FOnAppInfoFinished AppInfoFinishedDelegate;
};

namespace SandwichSteam::Editor
{
	/**
	 * For the Fetch App Info buttons: shows the confirm window (the command that runs, where the result is saved, what happens
	 * with it in this Mode, the current settings and blocking checks), then starts the fetch when confirmed. False when cancelled
	 * or it could not start (notified). The Setup page's automatic fetch does not go through here.
	 */
	bool ConfirmAndFetchAppInfo(ESteamCmdTaskMode Mode);

	/**
	 * For the Download/Install SteamCMD buttons: shows the confirm window (download URL, where it is extracted, the self-update,
	 * what happens with the SteamCMD path in this Mode, the existing install and blocking checks), then starts the download when
	 * confirmed. False when cancelled or it could not start (notified).
	 */
	bool ConfirmAndDownloadSteamCmd(ESteamCmdTaskMode Mode);
}
