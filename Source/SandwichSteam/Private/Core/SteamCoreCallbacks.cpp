// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamCoreCallbacks.h"
#include "Core/SteamCoreSubsystem.h"
#include "Core/SteamLog.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

FSteamCoreCallbacks::FSteamCoreCallbacks(USteamCoreSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	ServersConnectedCallback.Register(this, &FSteamCoreCallbacks::OnSteamServersConnected);
	ServersDisconnectedCallback.Register(this, &FSteamCoreCallbacks::OnSteamServersDisconnected);
	ShutdownCallback.Register(this, &FSteamCoreCallbacks::OnSteamShutdown);
}

// The three handlers below run on Steam's callback thread. Copy the payload, dispatch, return.

void FSteamCoreCallbacks::OnSteamServersConnected(SteamServersConnected_t* /*Payload*/)
{
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [](USteamCoreSubsystem& Core)
	{
		Core.HandleSteamServersConnected();
	});
}

void FSteamCoreCallbacks::OnSteamServersDisconnected(SteamServersDisconnected_t* Payload)
{
	const int32 NativeResult = static_cast<int32>(Payload->m_eResult);
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [NativeResult](USteamCoreSubsystem& Core)
	{
		Core.HandleSteamServersDisconnected(NativeResult);
	});
}

void FSteamCoreCallbacks::OnSteamShutdown(SteamShutdown_t* /*Payload*/)
{
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [](USteamCoreSubsystem& Core)
	{
		Core.HandleSteamShutdown();
	});
}

#if SANDWICHSTEAM_WITH_DEBUG
void FSteamCoreCallbacks::RunThreadCheck()
{
	ISteamUserStats* UserStats = SteamUserStats();
	if (!UserStats)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Core.ThreadCheck: SteamUserStats() is not available."));
		return;
	}

	const uint32 GameThreadId = GGameThreadId;

	// Looking up a leaderboard that does not exist completes with "not found": harmless, but it is a real
	// CCallResult round trip through TSteamCallResult and the dispatcher.
	const SteamAPICall_t Call = UserStats->FindLeaderboard("SandwichSteamThreadCheck");
	const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> LocalDispatcher = Dispatcher;

	const auto* Item = TSteamCallResult<LeaderboardFindResult_t>::Start(ThreadCheckPending, Dispatcher, Call,
		[LocalDispatcher, GameThreadId](const LeaderboardFindResult_t&, bool bIOFailure)
		{
			const uint32 CallbackThreadId = LocalDispatcher->GetLastEnqueueThreadId();
			const bool bOnGameThread = LocalDispatcher->WasLastEnqueueOnGameThread();
			UE_LOG(LogSandwichSteam, Display,
				TEXT("Steam.Core.ThreadCheck: game thread id=%u, raw Steam callback ran on thread id=%u (%s the game thread)%s. Record this in PROGRESS.md [verify]."),
				GameThreadId, CallbackThreadId, bOnGameThread ? TEXT("SAME as") : TEXT("different from"),
				bIOFailure ? TEXT(", IO failure") : TEXT(""));
		});

	if (!Item)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Core.ThreadCheck: FindLeaderboard returned an invalid call handle."));
		return;
	}

	UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Core.ThreadCheck: request sent from game thread id=%u, waiting for the callback..."), GameThreadId);
}
#endif // SANDWICHSTEAM_WITH_DEBUG

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
