// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamFriendsAsyncActions.h"
#include "SteamFriendsSubsystem.h"

USteamReadFriendsAsyncAction* USteamReadFriendsAsyncAction::ReadSteamFriends(const UObject* WorldContextObject, ESteamFriendSource Source, ESteamFriendFilter Filter)
{
	USteamReadFriendsAsyncAction* Action = NewObject<USteamReadFriendsAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Source = Source;
	Action->Filter = Filter;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamReadFriendsAsyncAction::GetFeatureClass() const
{
	return USteamFriendsSubsystem::StaticClass();
}

void USteamReadFriendsAsyncAction::StartRequest()
{
	USteamFriendsSubsystem* FriendsSubsystem = Cast<USteamFriendsSubsystem>(GetFeature());
	if (!FriendsSubsystem)
	{
		return;
	}

	const FSteamResult Started = FriendsSubsystem->ReadFriends(Source, Filter,
		FSteamReadFriendsDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const TArray<FSteamFriendInfo>& Loaded)
		{
			if (Result.IsSuccess())
			{
				Friends = Loaded;
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

void USteamReadFriendsAsyncAction::BroadcastSuccess()
{
	OnFriendsRead.Broadcast(Friends);
	Super::BroadcastSuccess();
}
