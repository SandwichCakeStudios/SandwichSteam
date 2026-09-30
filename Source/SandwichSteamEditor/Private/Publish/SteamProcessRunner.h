// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FInteractiveProcess;

/**
 * Runs any command line process without blocking the game thread (UAT, pre and post steps). Output is split into lines and
 * delivered on the game thread, like FSteamCmdRunner does for SteamCMD (which needs the raw chunks for its prompts).
 */
class FSteamProcessRunner : public TSharedFromThis<FSteamProcessRunner>
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLine, const FString&);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFinished, int32 /*ReturnCode*/, bool /*bCanceled*/);

	FSteamProcessRunner();
	~FSteamProcessRunner();

	bool Start(const FString& Executable, const FString& Arguments, FString& OutError);

	/** Kills the process (and its children where the platform supports it). OnFinished still fires with bCanceled. */
	void Cancel();

	bool IsRunning() const { return bRunning; }

	FOnLine& OnLine() { return LineDelegate; }
	FOnFinished& OnFinished() { return FinishedDelegate; }

private:
	void HandleOutput(const FString& Output);
	void HandleCompleted(int32 ReturnCode, bool bCanceled);
	void HandleCanceled();
	void EmitPendingLine();

	TUniquePtr<FInteractiveProcess> Process;
	FOnLine LineDelegate;
	FOnFinished FinishedDelegate;
	FString Pending;
	bool bRunning = false;
	bool bCancelRequested = false;
};
