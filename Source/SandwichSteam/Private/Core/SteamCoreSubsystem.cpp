// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamCoreSubsystem.h"
#include "Core/SteamBackend.h"
#include "Core/SteamCoreCallbacks.h"
#include "Core/SteamLog.h"
#include "Core/SteamSDK.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	/** The GameInstance that owns Steam in this process. Steam state is process-global. */
	TWeakObjectPtr<const UGameInstance> GSteamOwner;

	/** A dedicated server has no Steam client; it is polled for a bounded time instead of waiting for a callback. */
	constexpr float ServerPollIntervalSeconds = 1.0f;
	constexpr int32 ServerPollMaxAttempts = 30;
}

const TCHAR* LexToString(ESteamState State)
{
	switch (State)
	{
	case ESteamState::Unavailable: return TEXT("Unavailable");
	case ESteamState::WaitingForSteam: return TEXT("WaitingForSteam");
	case ESteamState::Ready: return TEXT("Ready");
	case ESteamState::Offline: return TEXT("Offline");
	case ESteamState::ShuttingDown: return TEXT("ShuttingDown");
	default: return TEXT("Unknown");
	}
}

USteamCoreSubsystem* USteamCoreSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (WorldContext && GEngine)
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamCoreSubsystem>() : nullptr;
}

bool USteamCoreSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningCommandlet();
}

void USteamCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Dispatcher = MakeShared<FSteamCallbackDispatcher, ESPMode::ThreadSafe>();

#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (!SandwichSteam::IsSteamOSSAvailable(GetGameInstance()))
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Unavailable: the Steam Online Subsystem is not available."));
		return;
	}

	if (!TryClaimOwnership())
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Unavailable for this game instance: Steam is process-global and another game instance already owns it."));
		return;
	}

	// Dedicated servers have no Steam client, so there are no client callbacks to register.
	if (!IsRunningDedicatedServer())
	{
		Callbacks = MakeShared<FSteamCoreCallbacks>(this, Dispatcher.ToSharedRef());
	}

	EvaluateReadiness();
#else
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Unavailable: this build has no Steamworks support."));
#endif
}

void USteamCoreSubsystem::Deinitialize()
{
	if (ServerPollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ServerPollHandle);
		ServerPollHandle.Reset();
	}

	// Unregister raw callbacks first so nothing new reaches the dispatcher, then close it.
	Callbacks.Reset();
	if (Dispatcher.IsValid())
	{
		Dispatcher->Shutdown();
	}

	if (bIsSteamOwner)
	{
		SetSteamState(ESteamState::ShuttingDown);
		ReleaseOwnership();
	}

	Super::Deinitialize();
}

bool USteamCoreSubsystem::TryClaimOwnership()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UGameInstance* CurrentOwner = GSteamOwner.Get();

	if (CurrentOwner && CurrentOwner != GameInstance)
	{
		return false;
	}

	GSteamOwner = GameInstance;
	bIsSteamOwner = true;
	return true;
}

void USteamCoreSubsystem::ReleaseOwnership()
{
	if (bIsSteamOwner && GSteamOwner.Get() == GetGameInstance())
	{
		GSteamOwner.Reset();
	}
	bIsSteamOwner = false;
}

void USteamCoreSubsystem::EvaluateReadiness()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const bool bDedicatedServer = IsRunningDedicatedServer();
	const bool bReady = bDedicatedServer
		? SandwichSteam::IsSteamGameServerReady(GameInstance)
		: SandwichSteam::IsSteamClientReady(GameInstance);

	if (bReady)
	{
		SetSteamState(ESteamState::Ready);
		return;
	}

	SetSteamState(ESteamState::WaitingForSteam);

	if (bDedicatedServer)
	{
		ScheduleServerReadinessPoll();
	}
}

// Dormant: dedicated servers are not supported (future content, Documents/Plans/Improvements.md 4.1).
void USteamCoreSubsystem::ScheduleServerReadinessPoll()
{
	ServerPollAttempts = 0;
	ServerPollHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			++ServerPollAttempts;

			if (SandwichSteam::IsSteamGameServerReady(GetGameInstance()))
			{
				ServerPollHandle.Reset();
				SetSteamState(ESteamState::Ready);
				return false;
			}

			if (ServerPollAttempts >= ServerPollMaxAttempts)
			{
				ServerPollHandle.Reset();
				UE_LOG(LogSandwichSteam, Warning, TEXT("Steam game server did not log on within %d seconds. Steam features stay inactive."), ServerPollMaxAttempts);
				return false;
			}

			return true;
		}),
		ServerPollIntervalSeconds);
}

void USteamCoreSubsystem::SetSteamState(ESteamState NewState)
{
	const ESteamState OldState = SteamState;

	// ShuttingDown is terminal.
	if (NewState == OldState || OldState == ESteamState::ShuttingDown)
	{
		return;
	}

	SteamState = NewState;
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam state: %s -> %s"), LexToString(OldState), LexToString(NewState));

	if (NewState == ESteamState::Ready && !bJoinIntentParsed)
	{
		ParseLaunchJoinIntent();
	}

	OnSteamStateChanged.Broadcast(NewState, OldState);
}

void USteamCoreSubsystem::ParseLaunchJoinIntent()
{
	bJoinIntentParsed = true;

	// Steam launches the game with "+connect_lobby <id>" or "+connect <address>" when the user accepts an invite
	// or picks "Join Game" while the game is not running. [verify in Phase 9] FParse::Value handles the space separated form.
	const TCHAR* CommandLine = FCommandLine::Get();

	FString LobbyString;
	if (FParse::Value(CommandLine, TEXT("+connect_lobby "), LobbyString))
	{
		FSteamId LobbyId;
		if (FSteamId::FromString(LobbyString, LobbyId))
		{
			PendingJoinIntent.Type = ESteamJoinIntentType::Lobby;
			PendingJoinIntent.LobbyId = LobbyId;
			UE_LOG(LogSandwichSteam, Log, TEXT("Launch join intent: lobby %s"), *LobbyId.ToString());
		}
		else
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Ignoring +connect_lobby: '%s' is not a Steam ID."), *LobbyString);
		}
		return;
	}

	FString ServerAddress;
	if (FParse::Value(CommandLine, TEXT("+connect "), ServerAddress) && !ServerAddress.IsEmpty())
	{
		PendingJoinIntent.Type = ESteamJoinIntentType::Server;
		PendingJoinIntent.ServerAddress = ServerAddress;
		UE_LOG(LogSandwichSteam, Log, TEXT("Launch join intent: server %s"), *ServerAddress);
	}
}

bool USteamCoreSubsystem::ConsumePendingJoinIntent(FSteamJoinIntent& OutIntent)
{
	if (!PendingJoinIntent.IsValid())
	{
		OutIntent = FSteamJoinIntent();
		return false;
	}

	OutIntent = MoveTemp(PendingJoinIntent);
	PendingJoinIntent = FSteamJoinIntent();
	return true;
}

void USteamCoreSubsystem::HandleSteamServersConnected()
{
	UE_LOG(LogSandwichSteam, Verbose, TEXT("SteamServersConnected_t"));
	SetSteamState(ESteamState::Ready);
}

void USteamCoreSubsystem::HandleSteamServersDisconnected(int32 NativeResult)
{
	UE_LOG(LogSandwichSteam, Verbose, TEXT("SteamServersDisconnected_t (EResult %d)"), NativeResult);

	// Before the first logon Steam reports disconnects too; that is still "waiting", not "offline".
	if (SteamState == ESteamState::Ready)
	{
		SetSteamState(ESteamState::Offline);
	}
}

void USteamCoreSubsystem::HandleSteamShutdown()
{
	UE_LOG(LogSandwichSteam, Verbose, TEXT("SteamShutdown_t"));
	SetSteamState(ESteamState::ShuttingDown);
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamCoreSubsystem::BuildDebugString() const
{
	FString Out;
	Out += FString::Printf(TEXT("Steam state: %s\n"), LexToString(SteamState));
	Out += FString::Printf(TEXT("Game instance: %s (Steam owner: %s)\n"), *GetNameSafe(GetGameInstance()), bIsSteamOwner ? TEXT("yes") : TEXT("no"));
	Out += FString::Printf(TEXT("Steam OSS available: %s\n"), SandwichSteam::IsSteamOSSAvailable(GetGameInstance()) ? TEXT("yes") : TEXT("no"));
	Out += FString::Printf(TEXT("Client ready: %s, game server ready: %s\n"),
		SandwichSteam::IsSteamClientReady(GetGameInstance()) ? TEXT("yes") : TEXT("no"),
		SandwichSteam::IsSteamGameServerReady(GetGameInstance()) ? TEXT("yes") : TEXT("no"));

	const USteamToolSettings* Settings = USteamToolSettings::Get();
	Out += FString::Printf(TEXT("Configured AppId: %d"), Settings ? Settings->SteamAppId : 0);
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (SandwichSteam::IsSteamClientReady(GetGameInstance()))
	{
		Out += FString::Printf(TEXT(", running AppId: %u"), SteamUtils() ? SteamUtils()->GetAppID() : 0u);
	}
#endif
	Out += TEXT("\n");
	Out += FString::Printf(TEXT("SANDWICHSTEAM_WITH_STEAMWORKS=%d, SANDWICHSTEAM_WITH_DEBUG=%d\n"), SANDWICHSTEAM_WITH_STEAMWORKS, SANDWICHSTEAM_WITH_DEBUG);

	if (Dispatcher.IsValid())
	{
		const FSteamCallbackDispatcher::FStats Stats = Dispatcher->GetStats();
		Out += FString::Printf(TEXT("Dispatcher: enqueued=%llu drained=%llu dropped=%llu pending=%d tickerRegistrations=%llu\n"),
			Stats.Enqueued, Stats.Drained, Stats.Dropped, Stats.Pending, Stats.TickerRegistrations);
	}

	switch (PendingJoinIntent.Type)
	{
	case ESteamJoinIntentType::Lobby:
		Out += FString::Printf(TEXT("Pending join intent: lobby %s"), *PendingJoinIntent.LobbyId.ToString());
		break;
	case ESteamJoinIntentType::Server:
		Out += FString::Printf(TEXT("Pending join intent: server %s"), *PendingJoinIntent.ServerAddress);
		break;
	default:
		Out += TEXT("Pending join intent: none");
		break;
	}

	return Out;
}

void USteamCoreSubsystem::RunThreadCheck()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (Callbacks.IsValid() && SteamState == ESteamState::Ready)
	{
		Callbacks->RunThreadCheck();
		return;
	}
#endif
	UE_LOG(LogSandwichSteam, Warning, TEXT("Steam.Core.ThreadCheck needs a Steam client in state Ready (current state: %s)."), LexToString(SteamState));
}
#endif // SANDWICHSTEAM_WITH_DEBUG
