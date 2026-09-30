// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "SteamFriendsTypes.h"
#include "Widgets/SCompoundWidget.h"

class USteamFriendsSubsystem;

/** Test page for the Friends feature: read friends (all, online, in this game, recent players, blocked), the coalescing counters, overlay pages and the friend groups. */
class SSandwichSteamFriendsTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamFriendsTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamFriendsSubsystem* GetFriends() const;

	FText BuildListText() const;

	FReply OnRead(ESteamFriendSource Source, ESteamFriendFilter Filter);
	FReply OnShowGroups();
	FReply OnOpenFriendsList();
	void HandleRead(const FSteamResult& Result, const TArray<FSteamFriendInfo>& List, double StartSeconds);

	TWeakObjectPtr<UObject> WorldContext;
	TArray<FSteamFriendInfo> List;
	FText Status;
};
