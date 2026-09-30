// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/SteamLiveCodingGuard.h"
#include "Publish/SteamPackager.h"
#include "Publish/SteamPublishPreflight.h"

class FSteamCmdRunner;
class FSteamProcessRunner;
struct FSteamCmdEvent;
struct FSteamCmdResult;

enum class ESteamPublishStepId : uint8
{
	PreSteps,
	Package,
	Vdf,
	Upload,
	PostSteps
};

enum class ESteamPublishStepState : uint8
{
	Pending,
	Running,
	Done,
	Skipped,
	Failed
};

struct FSteamPublishResult
{
	bool bSuccess = false;
	bool bCanceled = false;
	bool bDryRun = false;
	/** 0 when none was reported. */
	int64 BuildId = 0;
	FString Branch;
	FString Message;
	/** Full log of this run. */
	FString LogFile;
};

/**
 * One publish run: pre steps, package (UAT), generate VDF, upload (SteamCMD), post steps. Steps run one after another and each
 * step is asynchronous, so the game thread never blocks. All delegates fire on the game thread. Not started twice: create a new job.
 * Every line is also written to <Publish Directory>/Logs/publish_<time>.log. Failures go to the Message Log.
 * Pre-flight checks and confirmation dialogs belong to the caller (the panel).
 */
class FSteamPublishJob : public TSharedFromThis<FSteamPublishJob>
{
public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLog, const FString& /*Line*/, ESteamLogSeverity);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStepChanged, ESteamPublishStepId, ESteamPublishStepState);
	/** 0..1, or a negative value when the progress is unknown. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnProgress, float);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatus, const FText&);
	/** Steam Guard code needed: answer with SubmitGuardCode or Cancel. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGuardRequested, bool /*bMobile*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFinished, const FSteamPublishResult&);

	FSteamPublishJob();
	~FSteamPublishJob();

	static const TArray<ESteamPublishStepId>& GetAllSteps();
	static FText GetStepLabel(ESteamPublishStepId Step);

	bool Start(const FSteamPublishOptions& InOptions, FString& OutError);
	void Cancel();
	void SubmitGuardCode(const FString& Code);
	bool IsRunning() const { return bRunning; }

	FOnLog& OnLog() { return LogDelegate; }
	FOnStepChanged& OnStepChanged() { return StepDelegate; }
	FOnProgress& OnProgress() { return ProgressDelegate; }
	FOnStatus& OnStatus() { return StatusDelegate; }
	FOnGuardRequested& OnGuardRequested() { return GuardDelegate; }
	FOnFinished& OnFinished() { return FinishedDelegate; }

private:
	void Log(const FString& Line, ESteamLogSeverity Severity = ESteamLogSeverity::Info);
	void SetStepState(ESteamPublishStepId Step, ESteamPublishStepState State);
	void AdvanceStep();
	void StepSucceeded();
	void StepFailed(const FString& Message);
	void Finish(bool bSuccess, const FString& Message);

	bool ShouldSkip(ESteamPublishStepId Step) const;
	void BeginCommandQueue(const TArray<FSteamPublishStep>& Steps);
	void RunNextCommand();
	void BeginPackage();
	void RestoreLiveCoding();
	/** Overall 0..1 from the step weights, the finished steps and StepFraction of the running one. */
	void UpdateProgress();
	static float GetStepWeight(ESteamPublishStepId Step);
	void BeginVdf();
	void BeginUpload();

	void HandleProcessLine(const FString& Line);
	void HandleProcessFinished(int32 ReturnCode, bool bProcessCanceled);
	void HandleSteamCmdEvent(const FSteamCmdEvent& Event);
	void HandleSteamCmdFinished(const FSteamCmdResult& Result);

	FSteamPublishOptions Options;
	FSteamPublishResult Result;

	TArray<ESteamPublishStepId> Steps;
	int32 StepIndex = INDEX_NONE;
	bool bRunning = false;
	bool bCanceled = false;
	bool bUploadReached = false;
	/** 0..1 inside the running step; only steps that report it (upload, UAT phases) move it. */
	float StepFraction = 0.f;
	/** run_app_build reports percent twice: 0 = scanning content, 1 = uploading. */
	int32 UploadPhase = 0;

	TArray<FSteamPublishStep> CommandQueue;
	int32 CommandIndex = 0;

	FString AppVdfPath;
	/** Text of the final result when every step succeeded. */
	FString SuccessMessage;
	TArray<FString> ErrorLines;

	TSharedPtr<FSteamProcessRunner> ProcessRunner;
	TSharedPtr<FSteamCmdRunner> SteamCmdRunner;
	TUniquePtr<FArchive> LogFile;
	FSteamLiveCodingGuard LiveCodingGuard;

	FOnLog LogDelegate;
	FOnStepChanged StepDelegate;
	FOnProgress ProgressDelegate;
	FOnStatus StatusDelegate;
	FOnGuardRequested GuardDelegate;
	FOnFinished FinishedDelegate;
};
