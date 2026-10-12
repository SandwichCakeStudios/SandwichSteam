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
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "OnlineAuthInterfaceUtilsSteam.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemSteam.h"

namespace
{
	const TCHAR* LexResponseCode(uint8 Code)
	{
		switch (static_cast<ESteamAuthResponseCode>(Code))
		{
		case ESteamAuthResponseCode::OK:                  return TEXT("OK");
		case ESteamAuthResponseCode::NotConnectedToSteam: return TEXT("NotConnectedToSteam");
		case ESteamAuthResponseCode::NoOwnership:         return TEXT("NoOwnership");
		case ESteamAuthResponseCode::VACBanned:           return TEXT("VACBanned");
		case ESteamAuthResponseCode::LoggedInElsewhere:   return TEXT("LoggedInElsewhere");
		case ESteamAuthResponseCode::VACCheckTimeout:     return TEXT("VACCheckTimeout");
		case ESteamAuthResponseCode::TicketCancelled:     return TEXT("TicketCancelled");
		case ESteamAuthResponseCode::TicketAlreadyUsed:   return TEXT("TicketAlreadyUsed");
		case ESteamAuthResponseCode::TicketInvalid:       return TEXT("TicketInvalid");
		case ESteamAuthResponseCode::PublisherBanned:     return TEXT("PublisherBanned");
		case ESteamAuthResponseCode::FailedToCreateUser:  return TEXT("FailedToCreateUser");
		default:                                          return TEXT("Unknown");
		}
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
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: inactive, the SteamAuth packet handler is not in [PacketHandlerComponents]."));
		return false;
	}

	AuthUtils = Utils;
	Records.Reset();

	if (Utils->OnAuthenticationResultWithCodeDelegate.IsBound())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam auth: OnAuthenticationResultWithCodeDelegate is already bound by other code. Sandwich Steam leaves it alone and reports no results."));
	}
	else
	{
		Utils->OnAuthenticationResultWithCodeDelegate.BindUObject(this, &USteamAuthSubsystem::HandleAuthResult);
		bBoundResultDelegate = true;
	}

	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &USteamAuthSubsystem::HandlePostLogin);

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: active. The engine checks every joining player; failed players are kicked by the engine."));
	return true;
}

void USteamAuthSubsystem::ShutdownFeature()
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	PostLoginHandle.Reset();

	if (const FOnlineAuthSteamUtilsPtr Utils = AuthUtils.Pin())
	{
		if (bBoundResultDelegate && Utils->OnAuthenticationResultWithCodeDelegate.IsBoundToObject(this))
		{
			Utils->OnAuthenticationResultWithCodeDelegate.Unbind();
		}
	}

	bBoundResultDelegate = false;
	bPacketHandlerEnabled = false;
	AuthUtils.Reset();
	Records.Reset();
}

void USteamAuthSubsystem::HandleAuthResult(const FUniqueNetId& UserId, bool bWasSuccess, ESteamAuthResponseCode ResponseCode)
{
	FSteamId Player;
	FSteamId::FromUniqueNetId(UserId, Player);
	const uint8 Code = static_cast<uint8>(ResponseCode);

	if (IsInGameThread())
	{
		RecordAuthResult(Player, bWasSuccess, Code, true);
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<USteamAuthSubsystem>(this), Player, bWasSuccess, Code]()
	{
		if (USteamAuthSubsystem* Self = WeakThis.Get())
		{
			Self->RecordAuthResult(Player, bWasSuccess, Code, false);
		}
	});
}

void USteamAuthSubsystem::RecordAuthResult(FSteamId Player, bool bWasSuccess, uint8 ResponseCode, bool bOnGameThread)
{
	FAuthRecord& Record = Records.FindOrAdd(Player);
	Record.bSuccess = bWasSuccess;
	Record.ResponseCode = ResponseCode;
	Record.ResultSeconds = FPlatformTime::Seconds();
	Record.bResultOnGameThread = bOnGameThread;

	const TCHAR* Timing = Record.PostLoginSeconds > 0.0 ? TEXT("after PostLogin") : TEXT("before PostLogin");
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: result %s for %s, response %s (%u), %s, %s."),
		bWasSuccess ? TEXT("PASSED") : TEXT("FAILED"), *Player.ToString(), LexResponseCode(ResponseCode), ResponseCode,
		Timing, bOnGameThread ? TEXT("game thread") : TEXT("OFF the game thread"));
}

void USteamAuthSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || GameMode->GetGameInstance() != GetGameInstance() || !NewPlayer || NewPlayer->IsLocalController())
	{
		return;
	}

	const APlayerState* PlayerState = NewPlayer->PlayerState;
	FSteamId Player;
	if (!PlayerState || !PlayerState->GetUniqueId().IsValid() || !FSteamId::FromUniqueNetId(*PlayerState->GetUniqueId(), Player))
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: PostLogin for a player without a Steam ID."));
		return;
	}

	FAuthRecord& Record = Records.FindOrAdd(Player);
	Record.PostLoginSeconds = FPlatformTime::Seconds();
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam auth: PostLogin %s, auth result %s."), *Player.ToString(),
		Record.ResultSeconds > 0.0 ? TEXT("already known") : TEXT("still pending"));
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamAuthSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Auth: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	Report += FString::Printf(TEXT("  SteamAuth packet handler: %s, result delegate bound: %s\n"),
		bPacketHandlerEnabled ? TEXT("enabled") : TEXT("not enabled"), bBoundResultDelegate ? TEXT("yes") : TEXT("no"));
	Report += FString::Printf(TEXT("  Players seen: %d\n"), Records.Num());

	for (const TPair<FSteamId, FAuthRecord>& Pair : Records)
	{
		const FAuthRecord& Record = Pair.Value;
		if (Record.ResultSeconds <= 0.0)
		{
			Report += FString::Printf(TEXT("    %s: pending (PostLogin seen, no result yet)\n"), *Pair.Key.ToString());
			continue;
		}

		const FString Timing = Record.PostLoginSeconds > 0.0
			? FString::Printf(TEXT("result-PostLogin %+.2fs"), Record.ResultSeconds - Record.PostLoginSeconds)
			: FString(TEXT("no PostLogin yet"));
		Report += FString::Printf(TEXT("    %s: %s, %s, %s, %s\n"), *Pair.Key.ToString(),
			Record.bSuccess ? TEXT("passed") : TEXT("failed"), LexResponseCode(Record.ResponseCode), *Timing,
			Record.bResultOnGameThread ? TEXT("game thread") : TEXT("off game thread"));
	}
	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
