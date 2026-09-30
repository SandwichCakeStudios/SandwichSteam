// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamFriendsTypes.h"
#include "SteamFriendsLibrary.generated.h"

/** Synchronous Friends helpers. Every function has a safe default (0, false, empty) while the Friends feature is inactive. */
UCLASS()
class SANDWICHSTEAMFRIENDS_API USteamFriendsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "Number of friends of the local user. 0 while Steam is not ready."))
	static int32 GetSteamFriendCount(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the user is a friend of the local user."))
	static bool IsSteamFriend(const UObject* WorldContextObject, FSteamId User);

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "Reads one user right away. Returns false while Steam is not ready. Info Loaded is false while Steam has not delivered the user's data."))
	static bool GetSteamFriendInfo(const UObject* WorldContextObject, FSteamId User, FSteamFriendInfo& Info);

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "The friend groups (tags) of the local user with their members."))
	static TArray<FSteamFriendGroup> GetSteamFriendGroups(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Friends", meta = (ToolTip = "True when Change Flags (from the Friend State Changed event) contains the given kind of change."))
	static bool HasSteamFriendChange(int32 ChangeFlags, ESteamFriendChange Change);

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the Steam friends list in the overlay."))
	static FSteamResult OpenSteamFriendsList(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the Steam profile of a user in the overlay."))
	static FSteamResult OpenSteamProfile(const UObject* WorldContextObject, FSteamId User);

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens a Steam chat with a user in the overlay."))
	static FSteamResult OpenSteamChat(const UObject* WorldContextObject, FSteamId User);

	UFUNCTION(BlueprintCallable, Category = "Steam|Friends", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the add friend dialog for a user in the overlay. Steam has no call to send or accept friend requests from a game."))
	static FSteamResult OpenSteamAddFriend(const UObject* WorldContextObject, FSteamId User);
};
