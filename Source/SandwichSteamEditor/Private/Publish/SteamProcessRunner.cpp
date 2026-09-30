// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamProcessRunner.h"
#include "Async/Async.h"
#include "Misc/InteractiveProcess.h"

#define LOCTEXT_NAMESPACE "SandwichSteamProcessRunner"

FSteamProcessRunner::FSteamProcessRunner() = default;

FSteamProcessRunner::~FSteamProcessRunner()
{
	if (Process.IsValid())
	{
		Process->OnOutput().Unbind();
		Process->OnCompleted().Unbind();
		Process->OnCanceled().Unbind();
		Process->Cancel(true);
	}
}

bool FSteamProcessRunner::Start(const FString& Executable, const FString& Arguments, FString& OutError)
{
	check(IsInGameThread());
	if (bRunning)
	{
		OutError = LOCTEXT("Busy", "A process is already running.").ToString();
		return false;
	}

	Pending.Reset();
	bCancelRequested = false;

	// The process thread calls these delegates; they only forward to the game thread.
	const TWeakPtr<FSteamProcessRunner> WeakThis = AsShared();
	Process = MakeUnique<FInteractiveProcess>(Executable, Arguments, /*bInHidden*/ true, /*bInLongTime*/ true);
	Process->OnOutput().BindLambda([WeakThis](const FString& Output)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Output]()
		{
			if (const TSharedPtr<FSteamProcessRunner> Runner = WeakThis.Pin())
			{
				Runner->HandleOutput(Output);
			}
		});
	});
	Process->OnCompleted().BindLambda([WeakThis](int32 ReturnCode, bool bCanceled)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, ReturnCode, bCanceled]()
		{
			if (const TSharedPtr<FSteamProcessRunner> Runner = WeakThis.Pin())
			{
				Runner->HandleCompleted(ReturnCode, bCanceled);
			}
		});
	});
	// Cancel() ends the process on this path, not through OnCompleted: without this, HandleCompleted (and everything
	// downstream - OnFinished, IsRunning() going false) would never run for a canceled process.
	Process->OnCanceled().BindLambda([WeakThis]()
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (const TSharedPtr<FSteamProcessRunner> Runner = WeakThis.Pin())
			{
				Runner->HandleCanceled();
			}
		});
	});

	if (!Process->Launch())
	{
		Process.Reset();
		OutError = FText::Format(LOCTEXT("LaunchFailed", "Could not start {0}."), FText::FromString(Executable)).ToString();
		return false;
	}

	bRunning = true;
	return true;
}

void FSteamProcessRunner::Cancel()
{
	check(IsInGameThread());
	if (bRunning && Process.IsValid())
	{
		bCancelRequested = true;
		Process->Cancel(true);
	}
}

void FSteamProcessRunner::HandleOutput(const FString& Output)
{
	if (!bRunning)
	{
		return;
	}
	for (const TCHAR Char : Output)
	{
		if (Char == TEXT('\n') || Char == TEXT('\r'))
		{
			EmitPendingLine();
		}
		else
		{
			Pending.AppendChar(Char);
		}
	}
}

void FSteamProcessRunner::EmitPendingLine()
{
	if (!Pending.IsEmpty())
	{
		const FString Line = MoveTemp(Pending);
		Pending.Reset();
		LineDelegate.Broadcast(Line);
	}
}

void FSteamProcessRunner::HandleCompleted(int32 ReturnCode, bool bCanceled)
{
	if (!bRunning)
	{
		return;
	}
	EmitPendingLine();
	bRunning = false;
	FinishedDelegate.Broadcast(ReturnCode, bCanceled || bCancelRequested);
}

void FSteamProcessRunner::HandleCanceled()
{
	// No return code on this path; every caller ignores it once bCanceled is true.
	HandleCompleted(1, /*bCanceled*/ true);
}

#undef LOCTEXT_NAMESPACE
