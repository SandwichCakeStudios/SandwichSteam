// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamCmdSetupService.h"
#include "Containers/Ticker.h"
#include "Dashboard/SteamConfirmDialog.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Publish/SteamAppInfoData.h"
#include "Publish/SteamAppInfoFetcher.h"
#include "Publish/SteamCmdDownloadJob.h"
#include "Publish/SteamCmdRunner.h"
#include "Publish/SteamPublishActions.h"
#include "Publish/SteamPublishSettings.h"
#include "SandwichSteamEditor.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SandwichSteamCmdSetupService"

namespace
{
	TSharedPtr<FSteamCmdSetupService> Instance;

	/** A focus change sooner than this after the terminal opened is the terminal itself taking focus, not the user coming back. */
	constexpr double TerminalLoginGraceSeconds = 3.0;

	void Notify(const FText& Text, SNotificationItem::ECompletionState State, float Duration = 8.f)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = Duration;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	/** Absolute steamcmd path and trimmed account name, or false with OutError. */
	bool GetLoginSetup(FString& OutExe, FString& OutUser, FString& OutError)
	{
		const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
		OutExe = User->SteamCmdPath.FilePath;
		OutUser = User->SteamUsername.TrimStartAndEnd();

		if (OutExe.IsEmpty() || !IFileManager::Get().FileExists(*OutExe))
		{
			OutError = LOCTEXT("NoSteamCmd", "SteamCMD was not found. Install it first, or set its path on the SteamCMD page.").ToString();
			return false;
		}
		if (!FSteamCmdCommandLine::IsValidUsername(OutUser))
		{
			OutError = LOCTEXT("BadUser", "Enter your Steam account name first. Only letters, digits and _ . - @ are allowed.").ToString();
			return false;
		}
		return true;
	}
}

FSteamCmdSetupService::FSteamCmdSetupService() = default;
FSteamCmdSetupService::~FSteamCmdSetupService() = default;

FSteamCmdSetupService& FSteamCmdSetupService::Get()
{
	if (!Instance.IsValid())
	{
		Instance = MakeShared<FSteamCmdSetupService>();
		if (FSlateApplication::IsInitialized())
		{
			Instance->ActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged()
				.AddSP(Instance.ToSharedRef(), &FSteamCmdSetupService::HandleApplicationActivationChanged);
		}
	}
	return *Instance;
}

void FSteamCmdSetupService::Shutdown()
{
	if (!Instance.IsValid())
	{
		return;
	}

	Instance->CancelDownload();
	Instance->CancelLoginCheck();
	Instance->CancelAppInfoFetch();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(Instance->ActivationHandle);
	}
	Instance.Reset();
}

bool FSteamCmdSetupService::IsAnyRunning() const
{
	return IsDownloading() || IsCheckingLogin() || IsFetchingAppInfo();
}

bool FSteamCmdSetupService::ConfirmCancelForClose()
{
	if (!IsAnyRunning())
	{
		return true;
	}
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("CloseWhileRunning", "A SteamCMD task (download, login check or app info fetch) is still running. Cancel it and close the tab?"));
	if (Answer != EAppReturnType::Yes)
	{
		return false;
	}
	CancelDownload();
	CancelLoginCheck();
	CancelAppInfoFetch();
	return true;
}

bool FSteamCmdSetupService::AdoptDownloadedSteamCmd()
{
	USteamPublishUserSettings* User = GetMutableDefault<USteamPublishUserSettings>();
	if (!User->SteamCmdPath.FilePath.IsEmpty())
	{
		return false;
	}

	const TOptional<FSteamCmdManifest> Manifest = SandwichSteam::Editor::ReadSteamCmdManifest(SandwichSteam::Editor::GetSteamCmdRoot());
	if (!Manifest.IsSet() || Manifest->InstalledPath.IsEmpty() || !IFileManager::Get().FileExists(*Manifest->InstalledPath))
	{
		return false;
	}

	User->SteamCmdPath.FilePath = Manifest->InstalledPath;
	User->SaveConfig();
	Notify(FText::Format(LOCTEXT("Adopted", "Found SteamCMD from an earlier download and set it as the SteamCMD path: {0}"), FText::FromString(Manifest->InstalledPath)), SNotificationItem::CS_Success);
	ChangedDelegate.Broadcast();
	return true;
}

//~ Download

bool FSteamCmdSetupService::IsDownloading() const
{
	return DownloadJob.IsValid() && DownloadJob->IsRunning();
}

bool FSteamCmdSetupService::StartDownload(ESteamCmdTaskMode Mode, FString& OutError)
{
	if (IsDownloading())
	{
		OutError = LOCTEXT("DownloadRunning", "SteamCMD is already being installed.").ToString();
		return false;
	}

	DownloadMode = Mode;
	DownloadProgress = -1.f;
	DownloadStep = FText::GetEmpty();

	DownloadJob = MakeShared<FSteamCmdDownloadJob>();
	DownloadJob->OnProgress().AddSP(this, &FSteamCmdSetupService::HandleDownloadProgress);
	DownloadJob->OnFinished().AddSP(this, &FSteamCmdSetupService::HandleDownloadFinished);
	if (!DownloadJob->Start(OutError))
	{
		DownloadJob.Reset();
		return false;
	}

	ChangedDelegate.Broadcast();
	return true;
}

void FSteamCmdSetupService::CancelDownload()
{
	if (IsDownloading())
	{
		DownloadJob->Cancel();
	}
}

void FSteamCmdSetupService::HandleDownloadProgress(float Progress, const FText& StepLabel)
{
	DownloadProgress = Progress;
	DownloadStep = StepLabel;
}

void FSteamCmdSetupService::HandleDownloadFinished(bool bSuccess, const FText& Message, const FString& InstalledExePath)
{
	bool bPathApplied = false;
	if (bSuccess && DownloadMode == ESteamCmdTaskMode::Automatic && !InstalledExePath.IsEmpty())
	{
		USteamPublishUserSettings* User = GetMutableDefault<USteamPublishUserSettings>();
		User->SteamCmdPath.FilePath = InstalledExePath;
		User->SaveConfig();
		bPathApplied = true;
		Notify(FText::Format(LOCTEXT("InstalledAndSet", "SteamCMD installed and set as the SteamCMD path: {0}"), FText::FromString(InstalledExePath)), SNotificationItem::CS_Success);
	}
	else
	{
		Notify(Message, bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}

	DownloadFinishedDelegate.Broadcast(bSuccess, InstalledExePath, DownloadMode, bPathApplied);
	ChangedDelegate.Broadcast();
}

//~ Login

bool FSteamCmdSetupService::IsCheckingLogin() const
{
	return LoginRunner.IsValid() && LoginRunner->IsRunning();
}

ESteamSetupLoginState FSteamCmdSetupService::GetLoginState() const
{
	if (IsCheckingLogin())
	{
		return ESteamSetupLoginState::Checking;
	}
	const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
	if (User->SteamUsername.TrimStartAndEnd() != CheckedUsername || User->SteamCmdPath.FilePath != CheckedExePath)
	{
		return ESteamSetupLoginState::Unknown;
	}
	return LoginState;
}

void FSteamCmdSetupService::SetLoginState(ESteamSetupLoginState NewState)
{
	LoginState = NewState;
	ChangedDelegate.Broadcast();
}

void FSteamCmdSetupService::BeginTerminalLogin()
{
	if (!SandwichSteam::Editor::SandwichSteamCmdLoginTerminal())
	{
		return;
	}
	bAwaitingTerminalLogin = true;
	TerminalOpenedTime = FPlatformTime::Seconds();
}

bool FSteamCmdSetupService::StartLoginCheck(bool bInteractive, FString& OutError)
{
	if (IsCheckingLogin())
	{
		OutError = LOCTEXT("LoginRunning", "A login check is already running.").ToString();
		return false;
	}

	FString Exe, User;
	if (!GetLoginSetup(Exe, User, OutError))
	{
		return false;
	}

	bLoginInteractive = bInteractive;
	bLoginGuardRequested = false;
	CheckedUsername = User;
	CheckedExePath = Exe;

	LoginRunner = MakeShared<FSteamCmdRunner>();
	LoginRunner->OnEvent().AddSP(this, &FSteamCmdSetupService::HandleLoginEvent);
	LoginRunner->OnFinished().AddSP(this, &FSteamCmdSetupService::HandleLoginFinished);
	if (!LoginRunner->Start(Exe, FSteamCmdCommandLine::BuildLoginCheck(User), OutError))
	{
		LoginRunner.Reset();
		return false;
	}

	if (bInteractive)
	{
		Notify(LOCTEXT("LoginStarted", "Checking the SteamCMD login (a first run may update SteamCMD)..."), SNotificationItem::CS_Pending, 4.f);
	}
	ChangedDelegate.Broadcast();
	return true;
}

void FSteamCmdSetupService::CancelLoginCheck()
{
	if (IsCheckingLogin())
	{
		LoginRunner->Cancel();
	}
}

void FSteamCmdSetupService::HandleLoginEvent(const FSteamCmdEvent& Event)
{
	if (Event.Type == ESteamCmdEventType::Line)
	{
		UE_LOG(LogSandwichSteamEditor, Log, TEXT("steamcmd: %s"), *Event.Text);
		return;
	}
	if (Event.Type != ESteamCmdEventType::GuardEmailRequested && Event.Type != ESteamCmdEventType::GuardMobileRequested)
	{
		return;
	}

	if (!bLoginInteractive)
	{
		// Nobody asked for this check: no modal dialog out of nowhere. The page offers Log in instead.
		bLoginGuardRequested = true;
		LoginRunner->Cancel();
		return;
	}

	const bool bMobile = Event.Type == ESteamCmdEventType::GuardMobileRequested;
	const TWeakPtr<FSteamCmdRunner> WeakRunner = LoginRunner;
	// Next tick: the dialog is modal and must not run inside the event delivery.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakRunner, bMobile](float)
	{
		if (const TSharedPtr<FSteamCmdRunner> Runner = WeakRunner.Pin())
		{
			const TOptional<FString> Code = SandwichSteam::Editor::PromptSteamGuardCode(bMobile);
			if (Code.IsSet())
			{
				Runner->SubmitGuardCode(Code.GetValue());
			}
			else
			{
				Runner->Cancel();
			}
		}
		return false;
	}));
}

void FSteamCmdSetupService::HandleLoginFinished(const FSteamCmdResult& Result)
{
	// Exit code 0 with "Failed" is SteamCMD's harmless shutdown assertion (see FSteamAppInfoFetcher::HandleFinished).
	const bool bOk = Result.Outcome == ESteamCmdOutcome::Success
		|| (Result.Outcome == ESteamCmdOutcome::Failed && Result.ReturnCode == 0 && Result.bLoggedIn);

	ESteamSetupLoginState NewState = ESteamSetupLoginState::Failed;
	FText Message;
	if (bOk)
	{
		NewState = ESteamSetupLoginState::Ok;
		bAwaitingTerminalLogin = false;
		Message = LOCTEXT("LoginOk", "SteamCMD login works.");
	}
	else if (bLoginGuardRequested || Result.Outcome == ESteamCmdOutcome::NeedsPassword)
	{
		NewState = ESteamSetupLoginState::NeedsLogin;
		Message = LOCTEXT("LoginNeeded", "SteamCMD has no cached login. Use Log in and sign in once in the terminal.");
	}
	else if (Result.Outcome == ESteamCmdOutcome::Canceled)
	{
		NewState = ESteamSetupLoginState::Unknown;
		Message = LOCTEXT("LoginCanceled", "Login check cancelled.");
	}
	else if (Result.Outcome == ESteamCmdOutcome::LoginFailed)
	{
		Message = FText::Format(LOCTEXT("LoginFailed", "Steam login failed: {0}"), FText::FromString(Result.Message));
	}
	else
	{
		Message = FText::Format(LOCTEXT("LoginError", "SteamCMD failed (exit code {0}). {1}"), Result.ReturnCode, FText::FromString(Result.Message));
	}

	// Automatic checks only report success; their failures are shown on the Setup page.
	if (bLoginInteractive || bOk)
	{
		Notify(Message, bOk ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail, bOk ? 6.f : 10.f);
	}

	SetLoginState(NewState);
}

void FSteamCmdSetupService::HandleApplicationActivationChanged(bool bIsActive)
{
	if (!bIsActive || !bAwaitingTerminalLogin || IsCheckingLogin()
		|| FPlatformTime::Seconds() - TerminalOpenedTime < TerminalLoginGraceSeconds)
	{
		return;
	}

	// Next tick: start the process outside the focus change handling.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSPLambda(this, [this](float)
	{
		FString Error;
		StartLoginCheck(/*bInteractive*/ false, Error);
		return false;
	}));
}

//~ App info

bool FSteamCmdSetupService::IsFetchingAppInfo() const
{
	return AppInfoFetcher.IsValid() && AppInfoFetcher->IsRunning();
}

bool FSteamCmdSetupService::StartAppInfoFetch(ESteamCmdTaskMode Mode, FString& OutError)
{
	if (IsFetchingAppInfo())
	{
		OutError = LOCTEXT("AppInfoRunning", "App info is already being fetched.").ToString();
		return false;
	}

	AppInfoMode = Mode;
	AppInfoFetcher = MakeShared<FSteamAppInfoFetcher>();
	AppInfoFetcher->OnFinished().AddSP(this, &FSteamCmdSetupService::HandleAppInfoFinished);
	if (!AppInfoFetcher->Start(OutError))
	{
		AppInfoFetcher.Reset();
		return false;
	}

	Notify(LOCTEXT("AppInfoStarted", "Fetching app info with SteamCMD (a first run may update SteamCMD)..."), SNotificationItem::CS_Pending, 4.f);
	ChangedDelegate.Broadcast();
	return true;
}

void FSteamCmdSetupService::CancelAppInfoFetch()
{
	if (IsFetchingAppInfo())
	{
		AppInfoFetcher->Cancel();
	}
}

void FSteamCmdSetupService::HandleAppInfoFinished(bool bSuccess, const FText& Message, const FString& FilePath, const TSharedPtr<FJsonObject>& AppInfo)
{
	LastAppInfoSummary = Message;

	if (!bSuccess || AppInfoMode == ESteamCmdTaskMode::Confirm)
	{
		Notify(Message, bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
	else
	{
		USteamPublishSettings* Settings = GetMutableDefault<USteamPublishSettings>();
		const int32 AppId = Settings->GetAppId();
		FSteamAppInfoData Data;
		FString Error;
		if (!FSteamAppInfo::Extract(AppInfo, AppId, Data, Error))
		{
			LastAppInfoSummary = FText::FromString(Error);
			Notify(LastAppInfoSummary, SNotificationItem::CS_Fail);
		}
		else if (Data.Depots.IsEmpty())
		{
			LastAppInfoSummary = FText::Format(LOCTEXT("NoDepotsOnSteam", "Steam lists no depots for app {0}. Create them on the partner site (SteamPipe > Depots), publish the changes there, then fetch again."), AppId);
			Notify(LastAppInfoSummary, SNotificationItem::CS_Fail, 12.f);
		}
		else
		{
			const FSteamAppInfoMerge Merge = FSteamAppInfo::Preview(*Settings, Data);
			FSteamAppInfo::Apply(*Settings, Data);
			Settings->TryUpdateDefaultConfigFile();
			LastAppInfoSummary = FText::Format(LOCTEXT("AppInfoApplied", "Added {0} depot(s) and {1} branch(es) from Steam to the Publish settings."), Merge.NewDepotIds.Num(), Merge.NewBranches.Num());
			Notify(LastAppInfoSummary, SNotificationItem::CS_Success);
		}
	}

	AppInfoFinishedDelegate.Broadcast(bSuccess, FilePath, AppInfo, AppInfoMode);
	ChangedDelegate.Broadcast();
}

namespace SandwichSteam::Editor
{
	bool ConfirmAndFetchAppInfo(ESteamCmdTaskMode Mode)
	{
		FSteamCmdSetupService& Service = FSteamCmdSetupService::Get();
		const USteamPublishSettings* Settings = USteamPublishSettings::Get();
		const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
		const int32 AppId = Settings->GetAppId();
		const FString Username = User->SteamUsername.TrimStartAndEnd();
		const FString ExePath = User->SteamCmdPath.FilePath;
		const FText AppIdText = FText::AsNumber(AppId, &FNumberFormattingOptions::DefaultNoGrouping());

		FSteamConfirmRequest Request;
		Request.Title = LOCTEXT("FetchTitle", "Fetch app info from Steam");
		Request.Intro = FText::Format(LOCTEXT("FetchIntro", "Asks Steam which depots and branches app {0} has, using SteamCMD with the login it remembers. Nothing on Steam is changed."), AppIdText);
		Request.ConfirmLabel = LOCTEXT("FetchButton", "Fetch app info");
		Request.Footer = LOCTEXT("FetchFooter", "Takes a few seconds (longer when SteamCMD updates itself first). Cancel it from the same button while it runs.");
		if (Mode == ESteamCmdTaskMode::Automatic)
		{
			Request.BannerMark = ESteamConfirmMark::Info;
			Request.BannerText = LOCTEXT("FetchAutoBanner", "New depots and branches are added to the Publish settings right away. Existing entries are kept as they are.");
		}
		else
		{
			Request.BannerMark = ESteamConfirmMark::Ok;
			Request.BannerText = LOCTEXT("FetchConfirmBanner", "Read only. You are asked afterwards whether to add what was found.");
		}

		{
			FSteamConfirmSection& Steps = Request.AddSection(LOCTEXT("FetchSteps", "What will happen"));
			Steps.AddRow(LOCTEXT("FetchRun", "Run SteamCMD"),
				FText::FromString(FString::Printf(TEXT("%s %s"), *FPaths::GetCleanFilename(ExePath), *FSteamCmdCommandLine::BuildAppInfoPrint(Username, AppId))),
				LOCTEXT("FetchRunNote", "Uses the remembered login; no password is sent. If Steam asks for a Steam Guard code the fetch stops: log in again first."),
				ESteamConfirmMark::Info);
			Steps.AddRow(LOCTEXT("FetchSave", "Save the result"), FText::FromString(FSteamAppInfoFetcher::GetOutputPath(AppId)),
				LOCTEXT("FetchSaveNote", "As JSON, overwritten on every fetch."), ESteamConfirmMark::Info);
			Steps.AddRow(LOCTEXT("FetchApply", "Depots and branches"),
				Mode == ESteamCmdTaskMode::Automatic ? LOCTEXT("FetchApplyAuto", "Added to Sandwich Steam - Publish automatically") : LOCTEXT("FetchApplyAsk", "Shown to you first; added only if you agree"),
				LOCTEXT("FetchApplyNote", "Nothing is removed or overwritten. The settings file is DefaultEditor.ini (shared with your team)."),
				Mode == ESteamCmdTaskMode::Automatic ? ESteamConfirmMark::Warning : ESteamConfirmMark::Info);
		}

		{
			int32 EnabledDepots = 0;
			for (const FSteamPublishDepot& Depot : Settings->Depots)
			{
				EnabledDepots += Depot.bEnabled ? 1 : 0;
			}
			FSteamConfirmSection& Current = Request.AddSection(LOCTEXT("FetchCurrent", "Current settings"));
			Current.AddRow(LOCTEXT("FetchAppId", "Steam App ID"), AppIdText);
			Current.AddRow(LOCTEXT("FetchAccount", "Steam account"), Username.IsEmpty() ? LOCTEXT("FetchNoAccount", "(not set)") : FText::FromString(Username));
			Current.AddRow(LOCTEXT("FetchExe", "SteamCMD"), ExePath.IsEmpty() ? LOCTEXT("FetchNoExe", "(not set)") : FText::FromString(ExePath));
			Current.AddRow(LOCTEXT("FetchDepots", "Depots"), FText::Format(LOCTEXT("FetchDepotsValue", "{0} configured, {1} enabled"), Settings->Depots.Num(), EnabledDepots));
			Current.AddRow(LOCTEXT("FetchBranches", "Branches"), FText::AsNumber(Settings->Branches.Num()));
		}

		// Checks: errors block the button, warnings are worth knowing first.
		TArray<FSteamValidationCheck> Checks;
		auto AddCheck = [&Checks](const TCHAR* Id, ESteamCheckSeverity Severity, const FText& Label, const FText& Detail)
		{
			FSteamValidationCheck& Check = Checks.AddDefaulted_GetRef();
			Check.Id = Id;
			Check.Severity = Severity;
			Check.Label = Label;
			Check.Detail = Detail;
		};
		if (ExePath.IsEmpty() || !IFileManager::Get().FileExists(*ExePath))
		{
			AddCheck(TEXT("SteamCmd"), ESteamCheckSeverity::Error, LOCTEXT("CheckExe", "SteamCMD"), LOCTEXT("CheckExeMissing", "Not found. Install it on the SteamCMD or Setup page."));
		}
		if (!FSteamCmdCommandLine::IsValidUsername(Username))
		{
			AddCheck(TEXT("User"), ESteamCheckSeverity::Error, LOCTEXT("CheckUser", "Steam account"), LOCTEXT("CheckUserMissing", "Missing or not a valid account name."));
		}
		if (AppId <= 0)
		{
			AddCheck(TEXT("AppId"), ESteamCheckSeverity::Error, LOCTEXT("CheckAppId", "Steam App ID"), LOCTEXT("CheckAppIdMissing", "Not set. Enter it in Project Settings > Plugins > Sandwich Steam."));
		}
		else if (AppId == SpacewarAppId)
		{
			AddCheck(TEXT("AppId"), ESteamCheckSeverity::Warning, LOCTEXT("CheckAppId", "Steam App ID"), LOCTEXT("CheckAppIdSpacewar", "480 is Valve's Spacewar test app. Your account most likely has no access to it, so Steam returns no depots."));
		}
		const ESteamSetupLoginState Login = Service.GetLoginState();
		if (Login == ESteamSetupLoginState::NeedsLogin || Login == ESteamSetupLoginState::Failed)
		{
			AddCheck(TEXT("Login"), ESteamCheckSeverity::Warning, LOCTEXT("CheckLogin", "Steam login"), LOCTEXT("CheckLoginFailed", "The last login check this session failed. Log in again first, or the fetch will stop."));
		}
		if (Service.IsFetchingAppInfo())
		{
			AddCheck(TEXT("Running"), ESteamCheckSeverity::Error, LOCTEXT("CheckRunning", "Fetch"), LOCTEXT("CheckRunningDetail", "A fetch is already running."));
		}
		Request.Sections.Add(MakeChecksSection(LOCTEXT("FetchChecks", "Checks"), Checks, /*bIssuesOnly*/ true));
		if (Checks.ContainsByPredicate([](const FSteamValidationCheck& Check) { return Check.Severity == ESteamCheckSeverity::Error; }))
		{
			Request.BlockedReason = LOCTEXT("FetchBlocked", "Fix the errors in Checks first.");
		}

		if (!ShowConfirmDialog(Request).bConfirmed)
		{
			return false;
		}

		FString Error;
		if (!Service.StartAppInfoFetch(Mode, Error))
		{
			Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
			return false;
		}
		return true;
	}

	bool ConfirmAndDownloadSteamCmd(ESteamCmdTaskMode Mode)
	{
		FSteamCmdSetupService& Service = FSteamCmdSetupService::Get();
		const FString Url = USteamPublishSettings::Get()->GetSteamCmdDownloadUrl();
		const FString Root = GetSteamCmdRoot();
		const FString InstallDir = GetSteamCmdInstallDir();
		const FString CurrentPath = USteamPublishUserSettings::Get()->SteamCmdPath.FilePath;
		const bool bCurrentPathValid = !CurrentPath.IsEmpty() && IFileManager::Get().FileExists(*CurrentPath);
		const FString NewExePath = InstallDir / GetSteamCmdExecutableName();
		const TOptional<FSteamCmdManifest> Manifest = ReadSteamCmdManifest(Root);
		const bool bHasInstall = Manifest.IsSet() && !Manifest->InstalledPath.IsEmpty() && IFileManager::Get().FileExists(*Manifest->InstalledPath);

		FSteamConfirmRequest Request;
		Request.Title = bHasInstall ? LOCTEXT("DownloadTitleAgain", "Re-download SteamCMD") : LOCTEXT("DownloadTitle", "Download SteamCMD");
		Request.Intro = FText::Format(LOCTEXT("DownloadIntro", "Downloads Valve's official SteamCMD for this editor's platform ({0}), extracts it and lets it update itself once. No login, nothing on Steam is changed."),
			FText::FromString(GetHostPlatformFolderName()));
		Request.ConfirmLabel = bHasInstall ? LOCTEXT("DownloadButtonAgain", "Re-download") : LOCTEXT("DownloadButton", "Download");
		Request.Footer = LOCTEXT("DownloadFooter", "A small archive, then SteamCMD updates itself (can take a minute on the first run). Cancel stops it at any step.");
		if (Mode == ESteamCmdTaskMode::Automatic)
		{
			Request.BannerMark = ESteamConfirmMark::Info;
			Request.BannerText = LOCTEXT("DownloadAutoBanner", "When it finishes, the SteamCMD path in your user settings is set to the new install right away.");
		}
		else
		{
			Request.BannerMark = ESteamConfirmMark::Ok;
			Request.BannerText = LOCTEXT("DownloadConfirmBanner", "Your settings are not changed. You are asked afterwards whether to use it as the SteamCMD path.");
		}

		{
			FSteamConfirmSection& Steps = Request.AddSection(LOCTEXT("DownloadSteps", "What will happen"));
			Steps.AddRow(LOCTEXT("DownloadGet", "Download"), Url.IsEmpty() ? LOCTEXT("DownloadNoUrl", "(no URL for this platform)") : FText::FromString(Url),
				FText::Format(LOCTEXT("DownloadGetNote", "Saved to {0}. Change the URL in Sandwich Steam - Publish > SteamCMD Download if Valve moves it."),
					FText::FromString(Root / TEXT("Download") / FPaths::GetCleanFilename(Url))),
				Url.IsEmpty() ? ESteamConfirmMark::Error : ESteamConfirmMark::Info);
			Steps.AddRow(LOCTEXT("DownloadExtract", "Extract"), FText::FromString(InstallDir),
				bHasInstall ? LOCTEXT("DownloadExtractOverwrite", "Uses the OS's own tar. Files of the existing install are overwritten; your cached Steam login is kept.")
				            : LOCTEXT("DownloadExtractNote", "Uses the OS's own tar."),
				bHasInstall ? ESteamConfirmMark::Warning : ESteamConfirmMark::Info);
			Steps.AddRow(LOCTEXT("DownloadUpdate", "Self-update"), FText::FromString(FString::Printf(TEXT("%s +quit"), *GetSteamCmdExecutableName())),
				LOCTEXT("DownloadUpdateNote", "SteamCMD downloads its own latest files. No account is used."), ESteamConfirmMark::Info);
			Steps.AddRow(LOCTEXT("DownloadManifest", "Write manifest"), FText::FromString(Root / TEXT("Manifest.json")),
				LOCTEXT("DownloadManifestNote", "Remembers what was installed, so the dashboard finds it again."), ESteamConfirmMark::Info);
			Steps.AddRow(LOCTEXT("DownloadPath", "SteamCMD path"),
				Mode == ESteamCmdTaskMode::Automatic ? LOCTEXT("DownloadPathAuto", "Set automatically") : LOCTEXT("DownloadPathAsk", "Asked after the download"),
				LOCTEXT("DownloadPathNote", "Stored in your own EditorPerProjectUserSettings.ini (not shared). You can always set it by hand on the SteamCMD page."),
				Mode == ESteamCmdTaskMode::Automatic ? ESteamConfirmMark::Warning : ESteamConfirmMark::Info);
		}

		{
			FSteamConfirmSection& Current = Request.AddSection(LOCTEXT("DownloadCurrent", "Current state"));
			Current.AddRow(LOCTEXT("DownloadCurrentPath", "SteamCMD path"),
				CurrentPath.IsEmpty() ? LOCTEXT("DownloadPathNotSet", "(not set)") : FText::FromString(CurrentPath),
				!CurrentPath.IsEmpty() && !bCurrentPathValid ? LOCTEXT("DownloadPathMissing", "This file does not exist.") : FText::GetEmpty(),
				bCurrentPathValid ? ESteamConfirmMark::Ok : ESteamConfirmMark::Idle);
			Current.AddRow(LOCTEXT("DownloadExisting", "Earlier download"),
				bHasInstall ? FText::Format(LOCTEXT("DownloadExistingValue", "{0} ({1})"), FText::FromString(Manifest->InstalledPath), FText::FromString(Manifest->DownloadedUtc.ToString(TEXT("%Y-%m-%d %H:%M UTC"))))
				            : LOCTEXT("DownloadExistingNone", "None"),
				FText::GetEmpty(), bHasInstall ? ESteamConfirmMark::Ok : ESteamConfirmMark::Idle);
		}

		TArray<FSteamValidationCheck> Checks;
		auto AddCheck = [&Checks](const TCHAR* Id, ESteamCheckSeverity Severity, const FText& Label, const FText& Detail)
		{
			FSteamValidationCheck& Check = Checks.AddDefaulted_GetRef();
			Check.Id = Id;
			Check.Severity = Severity;
			Check.Label = Label;
			Check.Detail = Detail;
		};
		if (Url.IsEmpty())
		{
			AddCheck(TEXT("Url"), ESteamCheckSeverity::Error, LOCTEXT("CheckUrl", "Download URL"), LOCTEXT("CheckUrlMissing", "No URL is set for this platform. Set it in Sandwich Steam - Publish > SteamCMD Download."));
		}
		if (Service.IsDownloading())
		{
			AddCheck(TEXT("Running"), ESteamCheckSeverity::Error, LOCTEXT("CheckDownloadRunning", "Download"), LOCTEXT("CheckDownloadRunningDetail", "SteamCMD is already being installed."));
		}
		if (Service.IsCheckingLogin() || Service.IsFetchingAppInfo())
		{
			AddCheck(TEXT("Busy"), ESteamCheckSeverity::Warning, LOCTEXT("CheckBusy", "SteamCMD in use"), LOCTEXT("CheckBusyDetail", "A login check or app info fetch is running. Extracting over a running SteamCMD can fail; wait for it to finish."));
		}
		if (Mode == ESteamCmdTaskMode::Automatic && bCurrentPathValid && FPaths::ConvertRelativePathToFull(CurrentPath) != FPaths::ConvertRelativePathToFull(NewExePath))
		{
			AddCheck(TEXT("Replace"), ESteamCheckSeverity::Warning, LOCTEXT("CheckReplace", "SteamCMD path"), LOCTEXT("CheckReplaceDetail", "A different, working SteamCMD is already set. It will be replaced by the new install."));
		}
		Request.Sections.Add(MakeChecksSection(LOCTEXT("DownloadChecks", "Checks"), Checks, /*bIssuesOnly*/ true));
		if (Checks.ContainsByPredicate([](const FSteamValidationCheck& Check) { return Check.Severity == ESteamCheckSeverity::Error; }))
		{
			Request.BlockedReason = LOCTEXT("DownloadBlocked", "Fix the errors in Checks first.");
		}

		if (!ShowConfirmDialog(Request).bConfirmed)
		{
			return false;
		}

		FString Error;
		if (!Service.StartDownload(Mode, Error))
		{
			Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
			return false;
		}
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
