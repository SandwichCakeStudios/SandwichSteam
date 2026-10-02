// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamCmdRunner.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/InteractiveProcess.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SandwichSteamCmdRunner"

FSteamCmdRunner::FSteamCmdRunner() = default;

FSteamCmdRunner::~FSteamCmdRunner()
{
	if (Process.IsValid())
	{
		Process->OnOutput().Unbind();
		Process->OnCompleted().Unbind();
		Process->OnCanceled().Unbind();
		Process->Cancel(true);
	}
}

bool FSteamCmdRunner::Start(const FString& ExePath, const FString& Arguments, FString& OutError)
{
	check(IsInGameThread());
	if (bRunning)
	{
		OutError = LOCTEXT("AlreadyRunning", "SteamCMD is already running.").ToString();
		return false;
	}
	if (ExePath.IsEmpty() || !IFileManager::Get().FileExists(*ExePath))
	{
		OutError = LOCTEXT("NoExe", "SteamCMD was not found. Set the path in Project Settings > Sandwich Steam - Publish (User).").ToString();
		return false;
	}

	Parser = FSteamCmdOutputParser();
	Pending = FSteamCmdResult();
	bCancelRequested = false;
	bPasswordRequested = false;
	bLoginFailed = false;
	bErrorSeen = false;

	// The process thread calls these delegates. They only forward to the game thread, where the parser and all state live.
	const TWeakPtr<FSteamCmdRunner> WeakSelf = AsShared();
	Process = MakeUnique<FInteractiveProcess>(ExePath, Arguments, /*bInHidden*/ true, /*bInLongTime*/ true);
	Process->OnOutput().BindLambda([WeakSelf](const FString& Output)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, Output]()
		{
			if (const TSharedPtr<FSteamCmdRunner> Runner = WeakSelf.Pin())
			{
				Runner->HandleOutput(Output);
			}
		});
	});
	Process->OnCompleted().BindLambda([WeakSelf](int32 ReturnCode, bool bCanceled)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, ReturnCode, bCanceled]()
		{
			if (const TSharedPtr<FSteamCmdRunner> Runner = WeakSelf.Pin())
			{
				Runner->HandleCompleted(ReturnCode, bCanceled);
			}
		});
	});
	// Cancel() ends the process on this path, not through OnCompleted: without this, HandleCompleted (and everything
	// downstream - OnFinished, IsRunning() going false) would never run for a canceled process.
	Process->OnCanceled().BindLambda([WeakSelf]()
	{
		AsyncTask(ENamedThreads::GameThread, [WeakSelf]()
		{
			if (const TSharedPtr<FSteamCmdRunner> Runner = WeakSelf.Pin())
			{
				Runner->HandleCanceled();
			}
		});
	});

	if (!Process->Launch())
	{
		Process.Reset();
		OutError = LOCTEXT("LaunchFailed", "SteamCMD could not be started.").ToString();
		return false;
	}

	bRunning = true;
	return true;
}

void FSteamCmdRunner::Cancel()
{
	check(IsInGameThread());
	if (bRunning && Process.IsValid())
	{
		bCancelRequested = true;
		Process->Cancel(true);
	}
}

void FSteamCmdRunner::SubmitGuardCode(const FString& Code)
{
	check(IsInGameThread());
	if (bRunning && Process.IsValid())
	{
		Process->SendWhenReady(Code.TrimStartAndEnd() + TEXT("\n"));
	}
}

void FSteamCmdRunner::HandleOutput(const FString& Output)
{
	if (!bRunning)
	{
		return;
	}

	TArray<FSteamCmdEvent> Events;
	Parser.Feed(Output, Events);
	for (const FSteamCmdEvent& Event : Events)
	{
		HandleEvent(Event);
	}
}

void FSteamCmdRunner::HandleEvent(const FSteamCmdEvent& Event)
{
	switch (Event.Type)
	{
	case ESteamCmdEventType::LoginOk:
		Pending.bLoggedIn = true;
		break;
	case ESteamCmdEventType::LoginFailed:
		bLoginFailed = true;
		Pending.Message = Event.Text;
		break;
	case ESteamCmdEventType::PasswordRequested:
		// Never answered: stop and let the user log in once in a terminal.
		bPasswordRequested = true;
		break;
	case ESteamCmdEventType::BuildFinished:
		Pending.BuildId = Event.Number;
		break;
	case ESteamCmdEventType::Error:
		bErrorSeen = true;
		if (Pending.Message.IsEmpty())
		{
			Pending.Message = Event.Text;
		}
		break;
	default:
		break;
	}

	EventDelegate.Broadcast(Event);

	if (Event.Type == ESteamCmdEventType::PasswordRequested)
	{
		Cancel();
	}
}

void FSteamCmdRunner::HandleCompleted(int32 ReturnCode, bool bCanceled)
{
	if (!bRunning)
	{
		return;
	}

	// Output that was still buffered counts as a line.
	TArray<FSteamCmdEvent> Events;
	Parser.Flush(Events);
	for (const FSteamCmdEvent& Event : Events)
	{
		HandleEvent(Event);
	}

	bRunning = false;

	FSteamCmdResult Result = Pending;
	Result.ReturnCode = ReturnCode;
	if (bPasswordRequested)
	{
		Result.Outcome = ESteamCmdOutcome::NeedsPassword;
	}
	else if (bCanceled || bCancelRequested)
	{
		Result.Outcome = ESteamCmdOutcome::Canceled;
	}
	else if (bLoginFailed)
	{
		Result.Outcome = ESteamCmdOutcome::LoginFailed;
	}
	else if (ReturnCode != 0 || bErrorSeen)
	{
		Result.Outcome = ESteamCmdOutcome::Failed;
	}
	else
	{
		Result.Outcome = ESteamCmdOutcome::Success;
	}

	FinishedDelegate.Broadcast(Result);
}

void FSteamCmdRunner::HandleCanceled()
{
	// No return code on this path; HandleCompleted's own bCancelRequested check takes it from here.
	HandleCompleted(1, /*bCanceled*/ true);
}

#undef LOCTEXT_NAMESPACE
