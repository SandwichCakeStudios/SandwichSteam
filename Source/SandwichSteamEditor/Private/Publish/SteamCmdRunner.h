// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/SteamCmdOutputParser.h"

class FInteractiveProcess;

enum class ESteamCmdOutcome : uint8
{
	Success,
	Canceled,
	/** Login needs a password: the user has to log in once in a terminal. */
	NeedsPassword,
	LoginFailed,
	/** SteamCMD ended with errors or a non zero exit code. */
	Failed
};

struct FSteamCmdResult
{
	ESteamCmdOutcome Outcome = ESteamCmdOutcome::Failed;
	int32 ReturnCode = 0;
	bool bLoggedIn = false;
	/** 0 when no build finished. */
	int64 BuildId = 0;
	/** Reason for LoginFailed, or the first error line. */
	FString Message;
};

/**
 * Runs steamcmd without blocking the game thread and reports parsed events. Everything is delivered on the game thread.
 * The password is never sent. When SteamCMD asks for one, the process is stopped (NeedsPassword). Steam Guard prompts are
 * reported as events; the owner shows a dialog and answers with SubmitGuardCode or Cancel.
 */
class FSteamCmdRunner : public TSharedFromThis<FSteamCmdRunner>
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEvent, const FSteamCmdEvent&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFinished, const FSteamCmdResult&);

	FSteamCmdRunner();
	~FSteamCmdRunner();

	/** Starts steamcmd. Returns false with OutError when it cannot be launched. */
	bool Start(const FString& ExePath, const FString& Arguments, FString& OutError);

	/** Stops the process. OnFinished still fires (Canceled). */
	void Cancel();

	bool IsRunning() const { return bRunning; }

	/** Answers a Steam Guard prompt. */
	void SubmitGuardCode(const FString& Code);

	/** Every parsed event (game thread). */
	FOnEvent& OnEvent() { return EventDelegate; }

	/** Once per Start (game thread). */
	FOnFinished& OnFinished() { return FinishedDelegate; }

private:
	void HandleOutput(const FString& Output);
	void HandleCompleted(int32 ReturnCode, bool bCanceled);
	void HandleCanceled();
	void HandleEvent(const FSteamCmdEvent& Event);

	TUniquePtr<FInteractiveProcess> Process;
	FSteamCmdOutputParser Parser;
	FOnEvent EventDelegate;
	FOnFinished FinishedDelegate;

	FSteamCmdResult Pending;
	bool bRunning = false;
	bool bCancelRequested = false;
	bool bPasswordRequested = false;
	bool bLoginFailed = false;
	bool bErrorSeen = false;
};
