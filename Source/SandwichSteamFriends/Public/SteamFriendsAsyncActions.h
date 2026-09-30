// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "SteamFriendsTypes.h"
#include "SteamFriendsAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamFriendsReadAsyncDelegate, const TArray<FSteamFriendInfo>&, Friends);

/** Reads the friends, the blocked users or the recent players of the local user. */
UCLASS()
class SANDWICHSTEAMFRIENDS_API USteamReadFriendsAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the users, online ones first. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Friends", meta = (ToolTip = "Called with the users that passed the filter. Online users come first, then by name."))
	FSteamFriendsReadAsyncDelegate OnFriendsRead;

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Read Steam Friends", ToolTip = "Reads a list of Steam users: friends, blocked users or recent players, optionally only the online ones or the ones playing this game. Avatars are not included: ask the User feature with the Steam ID."))
	static USteamReadFriendsAsyncAction* ReadSteamFriends(const UObject* WorldContextObject, ESteamFriendSource Source = ESteamFriendSource::Friends, ESteamFriendFilter Filter = ESteamFriendFilter::All);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	ESteamFriendSource Source = ESteamFriendSource::Friends;
	ESteamFriendFilter Filter = ESteamFriendFilter::All;
	TArray<FSteamFriendInfo> Friends;
};
