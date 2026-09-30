// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/User/SteamUserSubsystem.h"
#include "Core/SteamBackend.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Features/User/Backend/SteamUserBackend.h"
#include "Features/User/SteamAvatarCache.h"

#if SANDWICHSTEAM_WITH_DEBUG
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#endif

USteamUserSubsystem* USteamUserSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamUserSubsystem>() : nullptr;
}

FGameplayTag USteamUserSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_User;
}

bool USteamUserSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	const USteamToolSettings* Settings = USteamToolSettings::Get();
	AvatarCache = MakeShared<FSteamAvatarCache>(Settings ? Settings->MaxCachedAvatars : 64);
	Backend = MakeShared<FSteamUserBackend>(this, Dispatcher.ToSharedRef());
	return true;
#else
	return false;
#endif
}

void USteamUserSubsystem::ShutdownFeature()
{
	// Outstanding requests are cancelled by the async actions (they observe OnFeatureActiveChanged).
	if (Backend.IsValid())
	{
		for (const uint32 TicketHandle : IssuedTickets)
		{
			Backend->CancelTicket(TicketHandle);
		}
	}

	IssuedTickets.Reset();
	PendingTickets.Reset();
	PendingAvatars.Reset();
	AvatarCache.Reset();
	Backend.Reset();
}

bool USteamUserSubsystem::IsLoggedOn() const
{
	FSteamResult Result;
	return RequireActive(Result) && Backend->IsLoggedOn();
}

FSteamId USteamUserSubsystem::GetLocalSteamId() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetLocalSteamId() : FSteamId();
}

FString USteamUserSubsystem::GetPersonaName() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetPersonaName() : FString();
}

int32 USteamUserSubsystem::GetSteamLevel() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetSteamLevel() : 0;
}

bool USteamUserSubsystem::IsSubscribed() const
{
	FSteamResult Result;
	return RequireActive(Result) && Backend->IsSubscribed();
}

bool USteamUserSubsystem::IsSubscribedApp(int32 AppId) const
{
	FSteamResult Result;
	return AppId > 0 && RequireActive(Result) && Backend->IsSubscribedApp(static_cast<uint32>(AppId));
}

FSteamId USteamUserSubsystem::GetAppOwner() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetAppOwner() : FSteamId();
}

bool USteamUserSubsystem::IsFamilySharedLicense() const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return false;
	}

	const FSteamId Owner = Backend->GetAppOwner();
	return Owner.IsValid() && Owner != Backend->GetLocalSteamId();
}

FDateTime USteamUserSubsystem::GetEarliestPurchaseTime(int32 AppId) const
{
	FSteamResult Result;
	if (AppId <= 0 || !RequireActive(Result))
	{
		return FDateTime::FromUnixTimestamp(0);
	}
	return FDateTime::FromUnixTimestamp(Backend->GetEarliestPurchaseUnixTime(static_cast<uint32>(AppId)));
}

UTexture2D* USteamUserSubsystem::GetCachedAvatar(FSteamId UserId, ESteamAvatarSize Size)
{
	FSteamResult Result;
	if (!RequireActive(Result) || !AvatarCache.IsValid())
	{
		return nullptr;
	}
	return AvatarCache->Find(FSteamAvatarKey(UserId, Size));
}

FSteamResult USteamUserSubsystem::RequestAvatar(FSteamId UserId, ESteamAvatarSize Size, FSteamAvatarResultDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!UserId.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "AvatarInvalidId", "The Steam ID for the avatar request is invalid."));
	}

	UTexture2D* Texture = nullptr;
	switch (TryResolveAvatar(UserId, Size, Texture))
	{
	case ESteamAvatarFetch::Ready:
		OnComplete.ExecuteIfBound(FSteamResult::Success(), Texture);
		break;
	case ESteamAvatarFetch::Loading:
		{
			FPendingAvatar& Pending = PendingAvatars.AddDefaulted_GetRef();
			Pending.UserId = UserId;
			Pending.Size = Size;
			Pending.Callback = MoveTemp(OnComplete);
			UE_LOG(LogSandwichSteam, Verbose, TEXT("Avatar of %s is loading, %d request(s) pending."), *UserId.ToString(), PendingAvatars.Num());
		}
		break;
	case ESteamAvatarFetch::None:
	default:
		DeliverNoAvatar(OnComplete);
		break;
	}

	return FSteamResult::Success();
}

FSteamResult USteamUserSubsystem::RequestWebApiTicket(const FString& Identity, FSteamWebApiTicketDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!FSteamUserBackend::SupportsWebApiTicket())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
			NSLOCTEXT("SandwichSteam", "WebApiTicketUnsupported", "The Steamworks SDK shipped with this engine version has no GetAuthTicketForWebApi."));
	}

	const uint32 TicketHandle = Backend->RequestWebApiTicket(Identity);
	if (TicketHandle == 0)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "WebApiTicketFailed", "Steam refused to start the Web API ticket request."));
	}

	FPendingTicket& Pending = PendingTickets.AddDefaulted_GetRef();
	Pending.Handle = TicketHandle;
	Pending.Callback = MoveTemp(OnComplete);
	IssuedTickets.Add(TicketHandle);
	return FSteamResult::Success();
}

void USteamUserSubsystem::CancelWebApiTicket(int32 TicketHandle)
{
	FSteamResult Result;
	if (TicketHandle <= 0 || !RequireActive(Result))
	{
		return;
	}

	const uint32 Handle = static_cast<uint32>(TicketHandle);
	if (IssuedTickets.Remove(Handle) > 0)
	{
		Backend->CancelTicket(Handle);
	}
}

ESteamAvatarFetch USteamUserSubsystem::TryResolveAvatar(FSteamId UserId, ESteamAvatarSize Size, UTexture2D*& OutTexture)
{
	OutTexture = nullptr;
	if (!Backend.IsValid() || !AvatarCache.IsValid())
	{
		return ESteamAvatarFetch::None;
	}

	const FSteamAvatarKey Key(UserId, Size);
	OutTexture = AvatarCache->Find(Key);
	if (OutTexture)
	{
		return ESteamAvatarFetch::Ready;
	}

	FSteamAvatarPixels Pixels;
	const ESteamAvatarFetch Fetch = Backend->FetchAvatar(UserId, Size, Pixels);
	if (Fetch == ESteamAvatarFetch::Ready)
	{
		OutTexture = AvatarCache->Add(Key, Pixels);
		return OutTexture ? ESteamAvatarFetch::Ready : ESteamAvatarFetch::None;
	}
	return Fetch;
}

void USteamUserSubsystem::ResolvePendingAvatars(FSteamId ChangedUser)
{
	if (PendingAvatars.IsEmpty())
	{
		return;
	}

	// Callbacks may issue new requests, so work on a moved-out copy.
	TArray<FPendingAvatar> Work = MoveTemp(PendingAvatars);
	PendingAvatars.Reset();

	for (FPendingAvatar& Pending : Work)
	{
		if (!Pending.Callback.IsBound())
		{
			continue; // The requester is gone.
		}

		if (ChangedUser.IsValid() && Pending.UserId != ChangedUser)
		{
			PendingAvatars.Add(MoveTemp(Pending));
			continue;
		}

		UTexture2D* Texture = nullptr;
		switch (TryResolveAvatar(Pending.UserId, Pending.Size, Texture))
		{
		case ESteamAvatarFetch::Ready:
			Pending.Callback.ExecuteIfBound(FSteamResult::Success(), Texture);
			break;
		case ESteamAvatarFetch::Loading:
			PendingAvatars.Add(MoveTemp(Pending));
			break;
		case ESteamAvatarFetch::None:
		default:
			DeliverNoAvatar(Pending.Callback);
			break;
		}
	}
}

void USteamUserSubsystem::DeliverNoAvatar(const FSteamAvatarResultDelegate& Callback) const
{
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	UTexture2D* DefaultAvatar = Settings ? Settings->DefaultAvatar.LoadSynchronous() : nullptr;
	if (DefaultAvatar)
	{
		Callback.ExecuteIfBound(FSteamResult::Success(), DefaultAvatar);
	}
	else
	{
		Callback.ExecuteIfBound(FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
			NSLOCTEXT("SandwichSteam", "AvatarNone", "This Steam user has no avatar and no DefaultAvatar is set in the Sandwich Steam settings.")), nullptr);
	}
}

void USteamUserSubsystem::HandlePersonaStateChange(FSteamId UserId, bool bNameChanged, bool bAvatarChanged)
{
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return;
	}

	if (bNameChanged && UserId == Backend->GetLocalSteamId())
	{
		OnPersonaNameChanged.Broadcast(Backend->GetPersonaName());
	}

	if (bAvatarChanged && AvatarCache.IsValid())
	{
		AvatarCache->Invalidate(UserId);
	}

	// Persona data of a non-friend arrives through this callback, so pending avatar requests may be resolvable now.
	ResolvePendingAvatars(UserId);

	if (bAvatarChanged)
	{
		OnAvatarChanged.Broadcast(UserId);
	}
}

void USteamUserSubsystem::HandleAvatarImageLoaded(FSteamId UserId)
{
	if (IsFeatureActive())
	{
		ResolvePendingAvatars(UserId);
	}
}

void USteamUserSubsystem::HandleWebApiTicketResponse(uint32 TicketHandle, int32 NativeResult, TArray<uint8> Ticket)
{
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return;
	}

	const int32 PendingIndex = PendingTickets.IndexOfByPredicate([TicketHandle](const FPendingTicket& Pending) { return Pending.Handle == TicketHandle; });
	if (PendingIndex == INDEX_NONE)
	{
		return; // Not ours (another plugin requested it).
	}

	FPendingTicket Pending = MoveTemp(PendingTickets[PendingIndex]);
	PendingTickets.RemoveAtSwap(PendingIndex);

	constexpr int32 EResultOK = 1;
	if (NativeResult != EResultOK)
	{
		IssuedTickets.Remove(TicketHandle);
		Pending.Callback.ExecuteIfBound(FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "WebApiTicketError", "Steam could not create the Web API ticket."), NativeResult), FString(), 0);
		return;
	}

	if (!Pending.Callback.IsBound())
	{
		// Nobody is waiting any more (timed out or cancelled). Do not leak the ticket.
		IssuedTickets.Remove(TicketHandle);
		Backend->CancelTicket(TicketHandle);
		return;
	}

	Pending.Callback.Execute(FSteamResult::Success(), BytesToHex(Ticket.GetData(), Ticket.Num()), static_cast<int32>(TicketHandle));
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamUserSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.User: %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return Report;
	}

	FString OssStatus = TEXT("n/a");
	if (IOnlineSubsystem* Oss = SandwichSteam::GetSteamOSS(this))
	{
		const IOnlineIdentityPtr Identity = Oss->GetIdentityInterface();
		if (Identity.IsValid())
		{
			OssStatus = ELoginStatus::ToString(Identity->GetLoginStatus(0));
		}
	}

	const FSteamId Owner = Backend->GetAppOwner();
	Report += FString::Printf(TEXT("  Logged on (raw): %s, OSS login status: %s\n"), Backend->IsLoggedOn() ? TEXT("yes") : TEXT("no"), *OssStatus);
	Report += FString::Printf(TEXT("  Local user: %s (%s), level %d\n"), *Backend->GetLocalSteamId().ToString(), *Backend->GetPersonaName(), Backend->GetSteamLevel());
	Report += FString::Printf(TEXT("  Owns game: %s, license owner: %s%s\n"), Backend->IsSubscribed() ? TEXT("yes") : TEXT("no"), *Owner.ToString(),
		(Owner.IsValid() && Owner != Backend->GetLocalSteamId()) ? TEXT(" (Family Sharing)") : TEXT(""));
	Report += FString::Printf(TEXT("  Avatars cached: %d / %d, pending requests: %d\n"),
		AvatarCache.IsValid() ? AvatarCache->Num() : 0, AvatarCache.IsValid() ? AvatarCache->GetCapacity() : 0, PendingAvatars.Num());
	Report += FString::Printf(TEXT("  Web API ticket: %s, outstanding tickets: %d\n"),
		FSteamUserBackend::SupportsWebApiTicket() ? TEXT("supported") : TEXT("not in this SDK"), IssuedTickets.Num());
	return Report;
}
#endif
