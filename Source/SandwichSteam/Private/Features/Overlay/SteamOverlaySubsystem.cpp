// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Overlay/SteamOverlaySubsystem.h"
#include "Core/SteamBackend.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Features/Overlay/Backend/SteamOverlayBackend.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"

USteamOverlaySubsystem* USteamOverlaySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamOverlaySubsystem>() : nullptr;
}

FGameplayTag USteamOverlaySubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Overlay;
}

bool USteamOverlaySubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	Backend = MakeShared<FSteamOverlayBackend>();

	// The OSS raises the overlay opened/closed event; we only listen.
	IOnlineSubsystem* Oss = SandwichSteam::GetSteamOSS(this);
	const IOnlineExternalUIPtr ExternalUI = Oss ? Oss->GetExternalUIInterface() : nullptr;
	if (ExternalUI.IsValid())
	{
		ExternalUIHandle = ExternalUI->AddOnExternalUIChangeDelegate_Handle(
			FOnExternalUIChangeDelegate::CreateWeakLambda(this, [this](bool bIsOpening) { HandleExternalUIChange(bIsOpening); }));
	}
	else
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam OSS has no ExternalUI interface: OnOverlayActivated and the pause helper are unavailable."));
	}
	return true;
#else
	return false;
#endif
}

void USteamOverlaySubsystem::ShutdownFeature()
{
	if (ExternalUIHandle.IsValid())
	{
		IOnlineSubsystem* Oss = SandwichSteam::GetSteamOSS(this);
		const IOnlineExternalUIPtr ExternalUI = Oss ? Oss->GetExternalUIInterface() : nullptr;
		if (ExternalUI.IsValid())
		{
			ExternalUI->ClearOnExternalUIChangeDelegate_Handle(ExternalUIHandle);
		}
		ExternalUIHandle.Reset();
	}

	// Never leave the game paused because of a closed-down overlay.
	SetPausedByOverlay(false);
	bOverlayActive = false;
	Backend.Reset();
}

bool USteamOverlaySubsystem::IsOverlayEnabled() const
{
	FSteamResult Result;
	return RequireActive(Result) && Backend->IsOverlayEnabled();
}

FSteamResult USteamOverlaySubsystem::RequireOverlayEnabled() const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Backend->IsOverlayEnabled())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Unavailable,
			NSLOCTEXT("SandwichSteam", "OverlayDisabled", "The Steam overlay is not available. Start the game through Steam (or Standalone with the Steam client running) and make sure the overlay is enabled."));
	}
	return FSteamResult::Success();
}

FSteamResult USteamOverlaySubsystem::OpenDialog(ESteamOverlayDialog Dialog)
{
	FSteamResult Result = RequireOverlayEnabled();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const TCHAR* DialogName = nullptr;
	switch (Dialog)
	{
	case ESteamOverlayDialog::Friends:			DialogName = TEXT("friends"); break;
	case ESteamOverlayDialog::Community:		DialogName = TEXT("community"); break;
	case ESteamOverlayDialog::Players:			DialogName = TEXT("players"); break;
	case ESteamOverlayDialog::Settings:			DialogName = TEXT("settings"); break;
	case ESteamOverlayDialog::OfficialGameGroup: DialogName = TEXT("officialgamegroup"); break;
	case ESteamOverlayDialog::Stats:			DialogName = TEXT("stats"); break;
	case ESteamOverlayDialog::Achievements:		DialogName = TEXT("achievements"); break;
	case ESteamOverlayDialog::Store:
		Backend->OpenStore(0, static_cast<int32>(ESteamOverlayStoreFlag::None));
		return FSteamResult::Success();
	default:
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "OverlayNeedsArgument", "This overlay page needs an argument. Use Open Steam Overlay Target Dialog (user or lobby) or Open Steam Overlay Web Page (URL)."));
	}

	Backend->OpenDialog(DialogName);
	return FSteamResult::Success();
}

FSteamResult USteamOverlaySubsystem::OpenTargetDialog(ESteamOverlayDialog Dialog, FSteamId Target)
{
	FSteamResult Result = RequireOverlayEnabled();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!Target.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "OverlayInvalidTarget", "The Steam ID for the overlay page is invalid."));
	}

	switch (Dialog)
	{
	case ESteamOverlayDialog::UserProfile:	Backend->OpenUserDialog(TEXT("steamid"), Target.Value); break;
	case ESteamOverlayDialog::UserChat:		Backend->OpenUserDialog(TEXT("chat"), Target.Value); break;
	case ESteamOverlayDialog::AddFriend:	Backend->OpenUserDialog(TEXT("friendadd"), Target.Value); break;
	case ESteamOverlayDialog::InviteDialog:	Backend->OpenInviteDialog(Target.Value); break;
	default:
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "OverlayNoTarget", "Only UserProfile, UserChat, AddFriend and InviteDialog take a Steam ID."));
	}
	return FSteamResult::Success();
}

FSteamResult USteamOverlaySubsystem::OpenStore(int32 AppId, ESteamOverlayStoreFlag Flag)
{
	FSteamResult Result = RequireOverlayEnabled();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (AppId < 0)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "OverlayInvalidAppId", "The App ID for the store page is invalid."));
	}

	Backend->OpenStore(static_cast<uint32>(AppId), static_cast<int32>(Flag));
	return FSteamResult::Success();
}

FSteamResult USteamOverlaySubsystem::OpenWebPage(const FString& Url, bool bModal)
{
	FSteamResult Result = RequireOverlayEnabled();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (Url.IsEmpty())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "OverlayEmptyUrl", "The URL for the overlay browser is empty."));
	}

	Backend->OpenWebPage(Url, bModal);
	return FSteamResult::Success();
}

void USteamOverlaySubsystem::HandleExternalUIChange(bool bIsOpening)
{
	if (IsInGameThread())
	{
		HandleOverlayChanged(bIsOpening);
		return;
	}

	// The OSS normally calls this on the game thread. Marshal anyway so a change of that never breaks us.
	if (const USteamCoreSubsystem* SteamCoreSubsystem = GetCore())
	{
		if (const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = SteamCoreSubsystem->GetDispatcher())
		{
			SANDWICHSTEAM_DISPATCH(Dispatcher, TWeakObjectPtr<USteamOverlaySubsystem>(this), [bIsOpening](USteamOverlaySubsystem& Overlay)
			{
				Overlay.HandleOverlayChanged(bIsOpening);
			});
		}
	}
}

void USteamOverlaySubsystem::HandleOverlayChanged(bool bActive)
{
	if (!IsFeatureActive() || bActive == bOverlayActive)
	{
		return;
	}

	bOverlayActive = bActive;
	UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam overlay %s."), bActive ? TEXT("opened") : TEXT("closed"));

	SetPausedByOverlay(bActive && ShouldAutoPause());
	OnOverlayActivated.Broadcast(bActive);
}

bool USteamOverlaySubsystem::ShouldAutoPause() const
{
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	if (!Settings || !Settings->bAutoPauseOnOverlay)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Never pause other people's game: standalone, or a listen server nobody joined.
	switch (World->GetNetMode())
	{
	case NM_Standalone:
		return true;
	case NM_ListenServer:
		{
			const UNetDriver* NetDriver = World->GetNetDriver();
			return !NetDriver || NetDriver->ClientConnections.Num() == 0;
		}
	default:
		return false;
	}
}

void USteamOverlaySubsystem::SetPausedByOverlay(bool bPause)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bPausedByOverlay = false;
		return;
	}

	if (bPause)
	{
		// Respect a pause the game already has: it stays paused after the overlay closes.
		if (!bPausedByOverlay && !UGameplayStatics::IsGamePaused(World))
		{
			UGameplayStatics::SetGamePaused(World, true);
			bPausedByOverlay = true;
		}
	}
	else if (bPausedByOverlay)
	{
		bPausedByOverlay = false;
		if (UGameplayStatics::IsGamePaused(World))
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamOverlaySubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Overlay: %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return Report;
	}

	const USteamToolSettings* Settings = USteamToolSettings::Get();
	Report += FString::Printf(TEXT("  Overlay enabled: %s, open: %s\n"), Backend->IsOverlayEnabled() ? TEXT("yes") : TEXT("no"), bOverlayActive ? TEXT("yes") : TEXT("no"));
	Report += FString::Printf(TEXT("  OSS ExternalUI event bound: %s\n"), ExternalUIHandle.IsValid() ? TEXT("yes") : TEXT("no"));
	Report += FString::Printf(TEXT("  Auto pause: %s (paused by overlay now: %s)\n"),
		(Settings && Settings->bAutoPauseOnOverlay) ? TEXT("on") : TEXT("off"), bPausedByOverlay ? TEXT("yes") : TEXT("no"));
	return Report;
}
#endif
