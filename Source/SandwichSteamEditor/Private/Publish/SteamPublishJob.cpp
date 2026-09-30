// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishJob.h"
#include "HAL/FileManager.h"
#include "Logging/MessageLog.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "SandwichSteamEditor.h"
#include "Publish/SteamCmdRunner.h"
#include "Publish/SteamProcessRunner.h"
#include "Publish/SteamPublishHistory.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamPublishVdf.h"
#include "Publish/SteamVdfWriter.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPublishJob"

FSteamPublishJob::FSteamPublishJob() = default;

FSteamPublishJob::~FSteamPublishJob()
{
	// The runners kill their processes when they are destroyed.
	if (LogFile.IsValid())
	{
		LogFile->Close();
	}
}

const TArray<ESteamPublishStepId>& FSteamPublishJob::GetAllSteps()
{
	static const TArray<ESteamPublishStepId> All = {
		ESteamPublishStepId::PreSteps, ESteamPublishStepId::Package, ESteamPublishStepId::Vdf, ESteamPublishStepId::Upload, ESteamPublishStepId::PostSteps };
	return All;
}

FText FSteamPublishJob::GetStepLabel(ESteamPublishStepId Step)
{
	switch (Step)
	{
	case ESteamPublishStepId::PreSteps:
		return LOCTEXT("StepPre", "Pre steps");
	case ESteamPublishStepId::Package:
		return LOCTEXT("StepPackage", "Package");
	case ESteamPublishStepId::Vdf:
		return LOCTEXT("StepVdf", "Build scripts");
	case ESteamPublishStepId::Upload:
		return LOCTEXT("StepUpload", "Upload");
	default:
		return LOCTEXT("StepPost", "Post steps");
	}
}

bool FSteamPublishJob::Start(const FSteamPublishOptions& InOptions, FString& OutError)
{
	check(IsInGameThread());
	if (bRunning || StepIndex != INDEX_NONE)
	{
		OutError = LOCTEXT("AlreadyStarted", "This publish run was already started.").ToString();
		return false;
	}

	Options = InOptions;
	Result = FSteamPublishResult();
	Result.bDryRun = Options.bDryRun;
	Result.Branch = Options.Branch;
	Steps = GetAllSteps();
	SuccessMessage.Reset();

	// Log file next to the scripts.
	const FString LogDir = SandwichSteam::Publish::GetPublishDir() / TEXT("Logs");
	IFileManager::Get().MakeDirectory(*LogDir, true);
	Result.LogFile = LogDir / FString::Printf(TEXT("publish_%s.log"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	LogFile.Reset(IFileManager::Get().CreateFileWriter(*Result.LogFile, FILEWRITE_AllowRead));

	ProcessRunner = MakeShared<FSteamProcessRunner>();
	ProcessRunner->OnLine().AddSP(this, &FSteamPublishJob::HandleProcessLine);
	ProcessRunner->OnFinished().AddSP(this, &FSteamPublishJob::HandleProcessFinished);

	bRunning = true;
	Log(FString::Printf(TEXT("Publish %s started (branch: %s)"), Options.bDryRun ? TEXT("dry run") : TEXT("run"), Options.Branch.IsEmpty() ? TEXT("none") : *Options.Branch));
	AdvanceStep();
	return true;
}

void FSteamPublishJob::Cancel()
{
	check(IsInGameThread());
	if (!bRunning)
	{
		return;
	}
	bCanceled = true;
	Log(TEXT("Cancel requested"), ESteamLogSeverity::Warning);

	bool bStopped = false;
	if (ProcessRunner.IsValid() && ProcessRunner->IsRunning())
	{
		ProcessRunner->Cancel();
		bStopped = true;
	}
	if (SteamCmdRunner.IsValid() && SteamCmdRunner->IsRunning())
	{
		SteamCmdRunner->Cancel();
		bStopped = true;
	}
	// A running process reports back through its finished delegate; otherwise stop here.
	if (!bStopped)
	{
		Finish(false, LOCTEXT("Canceled", "Cancelled.").ToString());
	}
}

void FSteamPublishJob::SubmitGuardCode(const FString& Code)
{
	if (SteamCmdRunner.IsValid())
	{
		SteamCmdRunner->SubmitGuardCode(Code);
	}
}

void FSteamPublishJob::Log(const FString& Line, ESteamLogSeverity Severity)
{
	if (LogFile.IsValid())
	{
		const FTCHARToUTF8 Utf8(*(Line + LINE_TERMINATOR));
		LogFile->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
	}
	if (Severity == ESteamLogSeverity::Error && ErrorLines.Num() < 30)
	{
		ErrorLines.Add(Line);
	}
	LogDelegate.Broadcast(Line, Severity);
}

void FSteamPublishJob::SetStepState(ESteamPublishStepId Step, ESteamPublishStepState State)
{
	StepDelegate.Broadcast(Step, State);
}

bool FSteamPublishJob::ShouldSkip(ESteamPublishStepId Step) const
{
	const USteamPublishSettings* Settings = USteamPublishSettings::Get();
	switch (Step)
	{
	case ESteamPublishStepId::PreSteps:
		return Options.bDryRun || Settings->PreSteps.IsEmpty();
	case ESteamPublishStepId::Package:
		return Options.bDryRun || Options.bSkipPackaging;
	case ESteamPublishStepId::Vdf:
		return false;
	case ESteamPublishStepId::Upload:
		return Options.bDryRun || Options.bSkipUpload;
	default:
		return Options.bDryRun || Settings->PostSteps.IsEmpty();
	}
}

void FSteamPublishJob::AdvanceStep()
{
	if (!bRunning)
	{
		return;
	}

	++StepIndex;
	if (!Steps.IsValidIndex(StepIndex))
	{
		Finish(true, SuccessMessage.IsEmpty() ? LOCTEXT("Done", "Done.").ToString() : SuccessMessage);
		return;
	}

	const ESteamPublishStepId Step = Steps[StepIndex];
	if (ShouldSkip(Step))
	{
		SetStepState(Step, ESteamPublishStepState::Skipped);
		AdvanceStep();
		return;
	}

	SetStepState(Step, ESteamPublishStepState::Running);
	StepFraction = 0.f;
	UpdateProgress();
	switch (Step)
	{
	case ESteamPublishStepId::PreSteps:
		StatusDelegate.Broadcast(LOCTEXT("StatusPre", "Running pre steps..."));
		BeginCommandQueue(USteamPublishSettings::Get()->PreSteps);
		break;
	case ESteamPublishStepId::Package:
		StatusDelegate.Broadcast(LOCTEXT("StatusPackage", "Packaging with UAT (cook, stage, pak)..."));
		BeginPackage();
		break;
	case ESteamPublishStepId::Vdf:
		StatusDelegate.Broadcast(LOCTEXT("StatusVdf", "Writing build scripts..."));
		BeginVdf();
		break;
	case ESteamPublishStepId::Upload:
		StatusDelegate.Broadcast(LOCTEXT("StatusUpload", "Uploading to Steam..."));
		BeginUpload();
		break;
	default:
		StatusDelegate.Broadcast(LOCTEXT("StatusPost", "Running post steps..."));
		BeginCommandQueue(USteamPublishSettings::Get()->PostSteps);
		break;
	}
}

float FSteamPublishJob::GetStepWeight(ESteamPublishStepId Step)
{
	switch (Step)
	{
	case ESteamPublishStepId::PreSteps:
		return 0.05f;
	case ESteamPublishStepId::Package:
		return 0.6f;
	case ESteamPublishStepId::Vdf:
		return 0.02f;
	case ESteamPublishStepId::Upload:
		return 0.3f;
	default:
		return 0.03f;
	}
}

void FSteamPublishJob::UpdateProgress()
{
	// Skipped steps do not count, so a run without packaging still goes 0..1.
	float Total = 0.f;
	float Done = 0.f;
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (ShouldSkip(Steps[Index]))
		{
			continue;
		}
		const float Weight = GetStepWeight(Steps[Index]);
		Total += Weight;
		if (Index < StepIndex)
		{
			Done += Weight;
		}
		else if (Index == StepIndex)
		{
			Done += Weight * FMath::Clamp(StepFraction, 0.f, 1.f);
		}
	}
	ProgressDelegate.Broadcast(Total > 0.f ? FMath::Min(Done / Total, 0.99f) : 0.f);
}

void FSteamPublishJob::StepSucceeded()
{
	SetStepState(Steps[StepIndex], ESteamPublishStepState::Done);
	AdvanceStep();
}

void FSteamPublishJob::StepFailed(const FString& Message)
{
	if (!bRunning)
	{
		return;
	}
	Log(Message, ESteamLogSeverity::Error);
	SetStepState(Steps[StepIndex], ESteamPublishStepState::Failed);
	Finish(false, Message);
}

void FSteamPublishJob::Finish(bool bSuccess, const FString& Message)
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	RestoreLiveCoding();

	if (bCanceled)
	{
		bSuccess = false;
		if (Steps.IsValidIndex(StepIndex))
		{
			SetStepState(Steps[StepIndex], ESteamPublishStepState::Failed);
		}
	}

	Result.bSuccess = bSuccess;
	Result.bCanceled = bCanceled;
	Result.Message = bCanceled ? LOCTEXT("CanceledMsg", "Cancelled.").ToString() : Message;
	Log(FString::Printf(TEXT("%s: %s"), bSuccess ? TEXT("Finished") : TEXT("Stopped"), *Result.Message), bSuccess ? ESteamLogSeverity::Info : ESteamLogSeverity::Error);

	if (bUploadReached && !Options.bDryRun)
	{
		FSteamPublishHistoryEntry Entry;
		Entry.Time = FDateTime::Now();
		Entry.AppId = USteamPublishSettings::Get()->GetAppId();
		Entry.Branch = Options.Branch;
		Entry.BuildId = Result.BuildId;
		Entry.bSuccess = bSuccess;
		Entry.Message = Result.Message;
		FSteamPublishHistory::Append(Entry);
	}

	if (!bSuccess && !bCanceled)
	{
		FMessageLog MessageLog(FSteamProjectValidator::GetLogName());
		MessageLog.NewPage(FText::Format(LOCTEXT("MessagePage", "Publish {0}"), FText::AsDateTime(FDateTime::Now())));
		MessageLog.Error(FText::FromString(Result.Message));
		for (const FString& Line : ErrorLines)
		{
			MessageLog.Error(FText::FromString(Line));
		}
		MessageLog.Info(FText::Format(LOCTEXT("MessageLogFile", "Full log: {0}"), FText::FromString(Result.LogFile)));
		MessageLog.Open(EMessageSeverity::Error);
	}

	if (LogFile.IsValid())
	{
		LogFile->Close();
		LogFile.Reset();
	}

	ProgressDelegate.Broadcast(bSuccess ? 1.f : 0.f);
	FinishedDelegate.Broadcast(Result);
}

void FSteamPublishJob::BeginCommandQueue(const TArray<FSteamPublishStep>& InSteps)
{
	CommandQueue = InSteps;
	CommandIndex = 0;
	RunNextCommand();
}

void FSteamPublishJob::RunNextCommand()
{
	while (CommandQueue.IsValidIndex(CommandIndex) && CommandQueue[CommandIndex].Executable.TrimStartAndEnd().IsEmpty())
	{
		Log(TEXT("Step without an executable skipped"), ESteamLogSeverity::Warning);
		++CommandIndex;
	}
	if (!CommandQueue.IsValidIndex(CommandIndex))
	{
		StepSucceeded();
		return;
	}

	const FSteamPublishStep& Step = CommandQueue[CommandIndex];
	FString Executable, Arguments;
	FSteamPackager::BuildStepCommand(Step, FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), Executable, Arguments);
	Log(FString::Printf(TEXT("> %s %s"), *Step.Executable, *Step.Arguments));

	FString Error;
	if (!ProcessRunner->Start(Executable, Arguments, Error))
	{
		StepFailed(Error);
	}
}

void FSteamPublishJob::BeginPackage()
{
	const USteamPublishSettings* Settings = USteamPublishSettings::Get();

	FSteamPackageRequest Request;
	Request.ProjectFile = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	Request.Config = Settings->PackageConfig;
	Request.StagingDir = SandwichSteam::Publish::GetStagingDir();
	Request.TargetName = Settings->TargetName;
	for (const FSteamPublishDepot& Depot : Settings->Depots)
	{
		if (Depot.bEnabled)
		{
			Request.Platforms.AddUnique(Depot.Platform);
		}
	}
	if (Request.Platforms.IsEmpty())
	{
		StepFailed(LOCTEXT("NoPlatform", "No enabled depot to package.").ToString());
		return;
	}

	if (Options.bDisableLiveCoding)
	{
		FString LiveCodingMessage;
		if (LiveCodingGuard.Acquire(LiveCodingMessage))
		{
			Log(LiveCodingMessage, LiveCodingMessage.Contains(TEXT("Warning")) ? ESteamLogSeverity::Warning : ESteamLogSeverity::Info);
		}
	}

	FString Executable, Arguments;
	FSteamPackager::BuildUatCommand(Request, Executable, Arguments);
	Log(FString::Printf(TEXT("> %s %s"), *Executable, *Arguments));

	FString Error;
	if (!ProcessRunner->Start(Executable, Arguments, Error))
	{
		StepFailed(Error);
	}
}

void FSteamPublishJob::RestoreLiveCoding()
{
	FString Message;
	if (LiveCodingGuard.Release(Message))
	{
		Log(Message);
	}
}

void FSteamPublishJob::BeginVdf()
{
	SandwichSteam::Publish::FVdfFiles Files;
	FString Error;
	if (!SandwichSteam::Publish::WriteVdfFiles(*USteamPublishSettings::Get(), Options.Branch, Files, Error))
	{
		StepFailed(Error);
		return;
	}

	for (const FString& Warning : Files.Warnings)
	{
		// Missing content is only a problem for a real upload; the dry run just reports it.
		Log(Warning, ESteamLogSeverity::Warning);
	}
	AppVdfPath = Files.AppVdfPath;
	Log(FString::Printf(TEXT("Wrote %s and %d depot script(s)"), *Files.AppVdfPath, Files.DepotVdfPaths.Num()));

	if (Options.bDryRun)
	{
		SuccessMessage = FString::Printf(TEXT("Wrote %s (nothing was uploaded)."), *Files.AppVdfPath);
	}
	else if (Options.bSkipUpload)
	{
		SuccessMessage = FString::Printf(TEXT("Wrote %s and packaged the build (nothing was uploaded)."), *Files.AppVdfPath);
	}
	StepSucceeded();
}

void FSteamPublishJob::BeginUpload()
{
	bUploadReached = true;

	const USteamPublishSettings* Settings = USteamPublishSettings::Get();
	const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
	const FString UserName = User->SteamUsername.TrimStartAndEnd();
	if (!FSteamCmdCommandLine::IsValidUsername(UserName))
	{
		StepFailed(LOCTEXT("BadUser", "The Steam account name is missing or invalid.").ToString());
		return;
	}

	for (const FSteamPublishDepot& Depot : Settings->Depots)
	{
		if (Depot.bEnabled && !IFileManager::Get().DirectoryExists(*SandwichSteam::Publish::ResolveContentRoot(Depot)))
		{
			StepFailed(FString::Printf(TEXT("Depot %d: content folder %s does not exist. Package first."), Depot.DepotId, *SandwichSteam::Publish::ResolveContentRoot(Depot)));
			return;
		}
	}

	SteamCmdRunner = MakeShared<FSteamCmdRunner>();
	SteamCmdRunner->OnEvent().AddSP(this, &FSteamPublishJob::HandleSteamCmdEvent);
	SteamCmdRunner->OnFinished().AddSP(this, &FSteamPublishJob::HandleSteamCmdFinished);

	Log(FString::Printf(TEXT("> steamcmd %s"), *FSteamCmdCommandLine::BuildUpload(UserName, AppVdfPath)));
	FString Error;
	if (!SteamCmdRunner->Start(User->SteamCmdPath.FilePath, FSteamCmdCommandLine::BuildUpload(UserName, AppVdfPath), Error))
	{
		StepFailed(Error);
	}
}

void FSteamPublishJob::HandleProcessLine(const FString& Line)
{
	// UAT announces its phases ("********** COOK COMMAND STARTED **********"); each one moves the bar inside the Package step.
	if (Steps.IsValidIndex(StepIndex) && Steps[StepIndex] == ESteamPublishStepId::Package && Line.Contains(TEXT("COMMAND STARTED")))
	{
		float Fraction = -1.f;
		if (Line.Contains(TEXT("BUILD COMMAND")))
		{
			Fraction = 0.05f;
		}
		else if (Line.Contains(TEXT("COOK COMMAND")))
		{
			Fraction = 0.35f;
		}
		else if (Line.Contains(TEXT("STAGE COMMAND")))
		{
			Fraction = 0.8f;
		}
		else if (Line.Contains(TEXT("PACKAGE COMMAND")) || Line.Contains(TEXT("ARCHIVE COMMAND")))
		{
			Fraction = 0.9f;
		}
		if (Fraction > StepFraction)
		{
			StepFraction = Fraction;
			UpdateProgress();
		}
	}

	Log(Line, FSteamPackager::ClassifyLine(Line));
}

void FSteamPublishJob::HandleProcessFinished(int32 ReturnCode, bool bProcessCanceled)
{
	if (!bRunning || !Steps.IsValidIndex(StepIndex))
	{
		return;
	}
	if (bProcessCanceled || bCanceled)
	{
		bCanceled = true;
		Finish(false, FString());
		return;
	}
	if (ReturnCode != 0)
	{
		StepFailed(FString::Printf(TEXT("%s failed (exit code %d). See the log."), *GetStepLabel(Steps[StepIndex]).ToString(), ReturnCode));
		return;
	}

	if (Steps[StepIndex] == ESteamPublishStepId::Package)
	{
		RestoreLiveCoding();
		StepSucceeded();
	}
	else
	{
		++CommandIndex;
		RunNextCommand();
	}
}

void FSteamPublishJob::HandleSteamCmdEvent(const FSteamCmdEvent& Event)
{
	switch (Event.Type)
	{
	case ESteamCmdEventType::Line:
		Log(Event.Text);
		if (Event.Text.Contains(TEXT("Scanning content")))
		{
			UploadPhase = 0;
		}
		else if (Event.Text.Contains(TEXT("Uploading content")))
		{
			UploadPhase = 1;
			StepFraction = FMath::Max(StepFraction, 0.2f);
			UpdateProgress();
		}
		break;
	case ESteamCmdEventType::Error:
	case ESteamCmdEventType::LoginFailed:
		Log(Event.Text, ESteamLogSeverity::Error);
		break;
	case ESteamCmdEventType::SelfUpdate:
		StatusDelegate.Broadcast(LOCTEXT("StatusSelfUpdate", "Updating SteamCMD..."));
		break;
	case ESteamCmdEventType::LoginOk:
		StatusDelegate.Broadcast(LOCTEXT("StatusUploading", "Uploading to Steam..."));
		break;
	case ESteamCmdEventType::Progress:
		{
			// Scanning is the first 20% of the upload step, the transfer the rest.
			const float Percent = FMath::Clamp(Event.Value / 100.f, 0.f, 1.f);
			StepFraction = UploadPhase == 0 ? Percent * 0.2f : 0.2f + Percent * 0.8f;
			UpdateProgress();
		}
		break;
	case ESteamCmdEventType::GuardEmailRequested:
	case ESteamCmdEventType::GuardMobileRequested:
		StatusDelegate.Broadcast(LOCTEXT("StatusGuard", "Waiting for the Steam Guard code..."));
		GuardDelegate.Broadcast(Event.Type == ESteamCmdEventType::GuardMobileRequested);
		break;
	case ESteamCmdEventType::BuildFinished:
		Result.BuildId = Event.Number;
		break;
	default:
		break;
	}
}

void FSteamPublishJob::HandleSteamCmdFinished(const FSteamCmdResult& CmdResult)
{
	if (!bRunning)
	{
		return;
	}
	if (bCanceled || CmdResult.Outcome == ESteamCmdOutcome::Canceled)
	{
		bCanceled = true;
		Finish(false, FString());
		return;
	}

	Result.BuildId = CmdResult.BuildId;
	switch (CmdResult.Outcome)
	{
	case ESteamCmdOutcome::Success:
		if (CmdResult.BuildId > 0)
		{
			SuccessMessage = FString::Printf(TEXT("Build %lld uploaded%s."), CmdResult.BuildId, Options.Branch.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" to branch %s"), *Options.Branch));
		}
		else
		{
			Log(TEXT("SteamCMD ended without errors but did not report a BuildID. Check the build on the partner site."), ESteamLogSeverity::Warning);
			SuccessMessage = TEXT("Upload finished (no BuildID reported).");
		}
		StepSucceeded();
		break;
	case ESteamCmdOutcome::NeedsPassword:
		StepFailed(LOCTEXT("NeedsLogin", "SteamCMD has no cached login. Use 'Open login terminal' on the SteamCMD dashboard page, log in once and try again.").ToString());
		break;
	case ESteamCmdOutcome::LoginFailed:
		StepFailed(FString::Printf(TEXT("Steam login failed: %s"), *CmdResult.Message));
		break;
	default:
		StepFailed(CmdResult.Message.IsEmpty() ? FString::Printf(TEXT("SteamCMD failed (exit code %d). See the log."), CmdResult.ReturnCode) : CmdResult.Message);
		break;
	}
}

#undef LOCTEXT_NAMESPACE
