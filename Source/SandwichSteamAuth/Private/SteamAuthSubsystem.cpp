// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAuthSubsystem.h"
#include "Async/Async.h"
#include "Core/SteamBackend.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "OnlineAuthInterfaceUtilsSteam.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemSteam.h"
#include "SteamAuthSettings.h"

#define LOCTEXT_NAMESPACE "SandwichSteamAuth"

namespace
{
	ESteamAuthResponse MapResponseCode(ESteamAuthResponseCode Code)
	{
		switch (Code)
		{
		case ESteamAuthResponseCode::OK:                  return ESteamAuthResponse::OK;
		case ESteamAuthResponseCode::NotConnectedToSteam: return ESteamAuthResponse::NotConnectedToSteam;
		case ESteamAuthResponseCode::NoOwnership:         return ESteamAuthResponse::NoOwnership;
		case ESteamAuthResponseCode::VACBanned:           return ESteamAuthResponse::VACBanned;
		case ESteamAuthResponseCode::LoggedInElsewhere:   return ESteamAuthResponse::LoggedInElsewhere;
		case ESteamAuthResponseCode::VACCheckTimeout:     return ESteamAuthResponse::VACCheckTimeout;
		case ESteamAuthResponseCode::TicketCancelled:     return ESteamAuthResponse::TicketCancelled;
		case ESteamAuthResponseCode::TicketAlreadyUsed:   return ESteamAuthResponse::TicketAlreadyUsed;
		case ESteamAuthResponseCode::TicketInvalid:       return ESteamAuthResponse::TicketInvalid;
		case ESteamAuthResponseCode::PublisherBanned:     return ESteamAuthResponse::PublisherBanned;
		case ESteamAuthResponseCode::FailedToCreateUser:  return ESteamAuthResponse::FailedToCreateUser;
		default:                                          return ESteamAuthResponse::Unknown;
		}
	}

	FString LexResponse(ESteamAuthResponse Response)
	{
		return StaticEnum<ESteamAuthResponse>()->GetNameStringByValue(static_cast<int64>(Response));
	}

	FString LexStatus(ESteamAuthStatus Status)
	{
		return StaticEnum<ESteamAuthStatus>()->GetNameStringByValue(static_cast<int64>(Status));
	}

	FOnlineAuthSteamUtilsPtr FindAuthUtils(const UGameInstance* GameInstance)
	{
		IOnlineSubsystem* OnlineSubsystem = SandwichSteam::GetSteamOSS(GameInstance);
		if (!OnlineSubsystem || OnlineSubsystem->GetSubsystemName() != STEAM_SUBSYSTEM)
		{
			return nullptr;
		}
		return static_cast<FOnlineSubsystemSteam*>(OnlineSubsystem)->GetAuthInterfaceUtils();
	}

	bool GetSteamId(const APlayerState* PlayerState, FSteamId& OutId)
	{
		return PlayerState && PlayerState->GetUniqueId().IsValid() && FSteamId::FromUniqueNetId(*PlayerState->GetUniqueId(), OutId);
	}
}

USteamAuthSubsystem* USteamAuthSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamAuthSubsystem>() : nullptr;
}

FGameplayTag USteamAuthSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Auth;
}

bool USteamAuthSubsystem::InitializeFeature()
{
	const FOnlineAuthSteamUtilsPtr Utils = FindAuthUtils(GetGameInstance());
	if (!Utils.IsValid())
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: inactive, the default online subsystem is not Steam."));
		return false;
	}

	bPacketHandlerEnabled = Utils->IsSteamAuthEnabled();
	if (!bPacketHandlerEnabled)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: inactive, the SteamAuth packet handler is not in [PacketHandlerComponents] (turn on Enable Steam Authentication and run Configure Steam)."));
		return false;
	}

	const USteamAuthSettings* Settings = USteamAuthSettings::Get();
	if (Settings && !Settings->bEnableSteamAuthentication)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: the SteamAuth packet handler is in the ini but Enable Steam Authentication is off. The engine checks players anyway; results are reported."));
	}

	AuthUtils = Utils;
	Records.Reset();
	ActivePolicy = Settings ? Settings->FailurePolicy : ESteamAuthFailurePolicy::Kick;

	if (Utils->OnAuthenticationResultWithCodeDelegate.IsBound())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam auth: OnAuthenticationResultWithCodeDelegate is already bound by other code. Sandwich Steam leaves it alone and reports no results."));
	}
	else
	{
		Utils->OnAuthenticationResultWithCodeDelegate.BindUObject(this, &USteamAuthSubsystem::HandleAuthResult);
		bBoundResultDelegate = true;
	}

	if (ActivePolicy == ESteamAuthFailurePolicy::Custom)
	{
		if (Utils->OverrideFailureDelegate.IsBound())
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam auth: OverrideFailureDelegate is already bound by other code. The Custom policy is not applied and On Steam Auth Failed never fires; that code decides about failed players."));
		}
		else
		{
			// Binding it turns off the engine kick.
			Utils->OverrideFailureDelegate.BindUObject(this, &USteamAuthSubsystem::HandleAuthFailure);
			bBoundFailureDelegate = true;
		}
	}

	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &USteamAuthSubsystem::HandlePostLogin);
	LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &USteamAuthSubsystem::HandleLogout);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &USteamAuthSubsystem::HandleWorldCleanup);

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: active, policy %s. %s"),
		ActivePolicy == ESteamAuthFailurePolicy::Custom ? TEXT("Custom") : TEXT("Kick"),
		bBoundFailureDelegate ? TEXT("Failed players are not kicked automatically; On Steam Auth Failed decides.") : TEXT("Failed players are kicked by the engine."));
	return true;
}

void USteamAuthSubsystem::ShutdownFeature()
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	PostLoginHandle.Reset();
	LogoutHandle.Reset();
	WorldCleanupHandle.Reset();

	if (const FOnlineAuthSteamUtilsPtr Utils = AuthUtils.Pin())
	{
		if (bBoundResultDelegate && Utils->OnAuthenticationResultWithCodeDelegate.IsBoundToObject(this))
		{
			Utils->OnAuthenticationResultWithCodeDelegate.Unbind();
		}
		if (bBoundFailureDelegate && Utils->OverrideFailureDelegate.IsBoundToObject(this))
		{
			Utils->OverrideFailureDelegate.Unbind();
		}
	}

	bBoundResultDelegate = false;
	bBoundFailureDelegate = false;
	bPacketHandlerEnabled = false;
	ActivePolicy = ESteamAuthFailurePolicy::Kick;
	AuthUtils.Reset();
	Records.Reset();
}

ESteamAuthStatus USteamAuthSubsystem::GetAuthStatus(FSteamId Player) const
{
	const FAuthRecord* Record = Records.Find(Player);
	return Record ? Record->Status : ESteamAuthStatus::NotChecked;
}

bool USteamAuthSubsystem::GetLastResult(FSteamId Player, FSteamAuthResult& OutResult) const
{
	const FAuthRecord* Record = Records.Find(Player);
	if (!Record || !Record->bHasResult)
	{
		return false;
	}

	OutResult.Player = Player;
	OutResult.bSuccess = Record->Status == ESteamAuthStatus::Passed;
	OutResult.Response = Record->Response;
	return true;
}

bool USteamAuthSubsystem::IsPlayerAuthenticated(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}

	// Players on this machine never go through the packet handler.
	const APlayerController* Controller = Cast<APlayerController>(PlayerState->GetOwningController());
	if (Controller && Controller->IsLocalController())
	{
		return true;
	}

	FSteamId Player;
	return GetSteamId(PlayerState, Player) && GetAuthStatus(Player) == ESteamAuthStatus::Passed;
}

FSteamResult USteamAuthSubsystem::KickPlayer(FSteamId Player, const FText& Reason)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Player.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, LOCTEXT("KickInvalidId", "Kick Steam Player needs a valid Steam ID."));
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!GameMode || !GameMode->GameSession)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_WrongScope, LOCTEXT("KickNotHost", "Only the host can kick players."));
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		FSteamId Id;
		if (!Controller || Controller->IsLocalController() || !GetSteamId(Controller->PlayerState, Id) || Id != Player)
		{
			continue;
		}

		if (GameMode->GameSession->KickPlayer(Controller, Reason))
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: kicked %s."), *Player.ToString());
			return FSteamResult::Success();
		}
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, LOCTEXT("KickRefused", "The game session refused to kick the player."));
	}

	return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, LOCTEXT("KickNotFound", "No connected player has this Steam ID."));
}

void USteamAuthSubsystem::HandleAuthResult(const FUniqueNetId& UserId, bool bWasSuccess, ESteamAuthResponseCode ResponseCode)
{
	FSteamId Player;
	FSteamId::FromUniqueNetId(UserId, Player);
	const ESteamAuthResponse Response = MapResponseCode(ResponseCode);

	// Verified 2026-10-13: the result arrives on the game thread. Kept as a fallback in case another engine version changes that.
	if (IsInGameThread())
	{
		RecordAuthResult(Player, bWasSuccess, Response, true);
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<USteamAuthSubsystem>(this), Player, bWasSuccess, Response]()
	{
		if (USteamAuthSubsystem* Self = WeakThis.Get())
		{
			Self->RecordAuthResult(Player, bWasSuccess, Response, false);
		}
	});
}

void USteamAuthSubsystem::HandleAuthFailure(const FUniqueNetId& UserId)
{
	FSteamId Player;
	FSteamId::FromUniqueNetId(UserId, Player);

	if (IsInGameThread())
	{
		RecordAuthFailure(Player);
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<USteamAuthSubsystem>(this), Player]()
	{
		if (USteamAuthSubsystem* Self = WeakThis.Get())
		{
			Self->RecordAuthFailure(Player);
		}
	});
}

void USteamAuthSubsystem::RecordAuthResult(FSteamId Player, bool bWasSuccess, ESteamAuthResponse Response, bool bOnGameThread)
{
	if (!bOnGameThread && !bWarnedOffGameThread)
	{
		bWarnedOffGameThread = true;
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam auth: results arrive OFF the game thread in this engine version; they are moved to the game thread."));
	}

	FAuthRecord& Record = Records.FindOrAdd(Player);
	Record.Status = bWasSuccess ? ESteamAuthStatus::Passed : ESteamAuthStatus::Failed;
	Record.bHasResult = true;
	Record.Response = Response;
	Record.ResultSeconds = FPlatformTime::Seconds();
	Record.bResultOnGameThread = bOnGameThread;

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: result %s for %s, response %s, %s."),
		bWasSuccess ? TEXT("PASSED") : TEXT("FAILED"), *Player.ToString(), *LexResponse(Response),
		Record.PostLoginSeconds > 0.0 ? TEXT("after PostLogin") : TEXT("before PostLogin"));

	FSteamAuthResult Result;
	Result.Player = Player;
	Result.bSuccess = bWasSuccess;
	Result.Response = Response;
	OnSteamAuthResult.Broadcast(Result);
}

void USteamAuthSubsystem::RecordAuthFailure(FSteamId Player)
{
	// Also reached without a result: a player who joined without authentication or lost it mid-game.
	FAuthRecord& Record = Records.FindOrAdd(Player);
	Record.Status = ESteamAuthStatus::Failed;

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: %s failed (Custom policy, not kicked)%s."), *Player.ToString(),
		Record.bHasResult ? TEXT("") : TEXT(", no Steam result: joined without authentication or lost it"));

	OnSteamAuthFailed.Broadcast(Player);
}

bool USteamAuthSubsystem::IsOwnGameMode(const AGameModeBase* GameMode) const
{
	return GameMode && GameMode->GetGameInstance() == GetGameInstance();
}

void USteamAuthSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!IsOwnGameMode(GameMode) || !NewPlayer || NewPlayer->IsLocalController())
	{
		return;
	}

	FSteamId Player;
	if (!GetSteamId(NewPlayer->PlayerState, Player))
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: PostLogin for a player without a Steam ID, not tracked."));
		return;
	}

	FAuthRecord& Record = Records.FindOrAdd(Player);
	Record.PostLoginSeconds = FPlatformTime::Seconds();
	if (Record.Status == ESteamAuthStatus::NotChecked)
	{
		Record.Status = ESteamAuthStatus::Pending;
	}
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: PostLogin %s, status %s."), *Player.ToString(), *LexStatus(Record.Status));
}

void USteamAuthSubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!IsOwnGameMode(GameMode) || !Exiting || Exiting->IsLocalController())
	{
		return;
	}

	FSteamId Player;
	if (GetSteamId(Exiting->PlayerState, Player))
	{
		Records.Remove(Player);
	}
}

void USteamAuthSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool /*bCleanupResources*/)
{
	// [verify] seamless travel keeps the table (bSessionEnded false); a map load reconnects every client, so they are checked again.
	if (bSessionEnded && World && World->GetGameInstance() == GetGameInstance())
	{
		Records.Reset();
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamAuthSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Auth: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));

	const USteamAuthSettings* Settings = USteamAuthSettings::Get();
	Report += FString::Printf(TEXT("  Setting Enable Steam Authentication: %s, policy setting: %s\n"),
		(Settings && Settings->bEnableSteamAuthentication) ? TEXT("on") : TEXT("off"),
		(Settings && Settings->FailurePolicy == ESteamAuthFailurePolicy::Custom) ? TEXT("Custom") : TEXT("Kick"));
	Report += FString::Printf(TEXT("  SteamAuth packet handler (engine): %s, active policy: %s\n"),
		bPacketHandlerEnabled ? TEXT("enabled") : TEXT("not enabled"),
		ActivePolicy == ESteamAuthFailurePolicy::Custom ? TEXT("Custom") : TEXT("Kick"));
	Report += FString::Printf(TEXT("  Bound: result delegate %s, failure override %s\n"),
		bBoundResultDelegate ? TEXT("yes") : TEXT("no"), bBoundFailureDelegate ? TEXT("yes (engine kick off)") : TEXT("no (engine kicks)"));
	Report += FString::Printf(TEXT("  Players: %d\n"), Records.Num());

	const double Now = FPlatformTime::Seconds();
	for (const TPair<FSteamId, FAuthRecord>& Pair : Records)
	{
		const FAuthRecord& Record = Pair.Value;
		FString Line = FString::Printf(TEXT("    %s: %s"), *Pair.Key.ToString(), *LexStatus(Record.Status));
		if (Record.bHasResult)
		{
			Line += FString::Printf(TEXT(", %s"), *LexResponse(Record.Response));
			if (Record.PostLoginSeconds > 0.0)
			{
				Line += FString::Printf(TEXT(", result-PostLogin %+.2fs"), Record.ResultSeconds - Record.PostLoginSeconds);
			}
			Line += Record.bResultOnGameThread ? TEXT(", game thread") : TEXT(", off game thread");
		}
		else if (Record.PostLoginSeconds > 0.0)
		{
			Line += FString::Printf(TEXT(", PostLogin %.1fs ago, no result yet"), Now - Record.PostLoginSeconds);
		}
		Report += Line + TEXT("\n");
	}
	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG

#undef LOCTEXT_NAMESPACE
