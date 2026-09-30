// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/User/SteamUserAsyncActions.h"
#include "Features/User/SteamUserSubsystem.h"

USteamGetAvatarAsyncAction* USteamGetAvatarAsyncAction::GetSteamAvatar(const UObject* WorldContextObject, FSteamId UserId, ESteamAvatarSize Size)
{
	USteamGetAvatarAsyncAction* Action = NewObject<USteamGetAvatarAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->UserId = UserId;
	Action->Size = Size;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamGetAvatarAsyncAction::GetFeatureClass() const
{
	return USteamUserSubsystem::StaticClass();
}

void USteamGetAvatarAsyncAction::StartRequest()
{
	USteamUserSubsystem* User = Cast<USteamUserSubsystem>(GetFeature());
	if (!User)
	{
		return;
	}

	const FSteamResult Started = User->RequestAvatar(UserId, Size,
		FSteamAvatarResultDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, UTexture2D* Texture)
		{
			if (Result.IsSuccess())
			{
				Avatar = Texture;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamGetAvatarAsyncAction::BroadcastSuccess()
{
	OnAvatarLoaded.Broadcast(Avatar);
	Super::BroadcastSuccess();
}

USteamGetWebApiTicketAsyncAction* USteamGetWebApiTicketAsyncAction::GetSteamWebApiTicket(const UObject* WorldContextObject, const FString& Identity)
{
	USteamGetWebApiTicketAsyncAction* Action = NewObject<USteamGetWebApiTicketAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Identity = Identity;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamGetWebApiTicketAsyncAction::GetFeatureClass() const
{
	return USteamUserSubsystem::StaticClass();
}

void USteamGetWebApiTicketAsyncAction::StartRequest()
{
	USteamUserSubsystem* User = Cast<USteamUserSubsystem>(GetFeature());
	if (!User)
	{
		return;
	}

	const FSteamResult Started = User->RequestWebApiTicket(Identity,
		FSteamWebApiTicketDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const FString& InTicketHex, int32 InTicketHandle)
		{
			if (Result.IsSuccess())
			{
				TicketHex = InTicketHex;
				TicketHandle = InTicketHandle;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamGetWebApiTicketAsyncAction::BroadcastSuccess()
{
	OnTicketReady.Broadcast(TicketHex, TicketHandle);
	Super::BroadcastSuccess();
}
