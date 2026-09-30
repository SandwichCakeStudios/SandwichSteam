// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamPresenceTypes.h"
#include "SteamPresenceLibrary.generated.h"

/**
 * Rich presence for Blueprints. Changes are staged and sent once per frame, so calling several of these in a row is cheap.
 * Every function returns a result: Steam.Error.FeatureDisabled when the Presence feature is not available, Steam.Error.InvalidArgument
 * or Steam.Error.QuotaExceeded when Steam's limits (30 keys, 64 bytes per key, 256 bytes per value) would be broken.
 */
UCLASS()
class SANDWICHSTEAMPRESENCE_API USteamPresenceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Presence", AutoCreateRefTerm = "Args", ToolTip = "Shows a status of the Steam App Definition to friends. Args are the values the status text substitutes, for example map = Harbor."))
	static FSteamResult SetSteamPresence(const UObject* WorldContextObject, FGameplayTag Status, const TMap<FName, FString>& Args);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence|By Token", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Args", DisplayName = "Set Steam Presence By Token", Keywords = "set steam presence status token rich", ToolTip = "Shows a status to friends by its localization token (for example #Status_InMatch), without an App Definition row. The token must exist in the rich presence file uploaded to Steamworks. Args are the values the status text substitutes."))
	static FSteamResult SetSteamPresenceByToken(const UObject* WorldContextObject, const FString& Token, const TMap<FName, FString>& Args);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Sets a free rich presence key, for example status (plain text that needs no localization file). An empty value removes the key."))
	static FSteamResult SetSteamPresenceValue(const UObject* WorldContextObject, FName Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Removes a rich presence key."))
	static FSteamResult RemoveSteamPresenceValue(const UObject* WorldContextObject, FName Key);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Groups the local user with others in the same party or match (steam_player_group). Steam shows the group and its size to friends. An empty group ID removes the group."))
	static FSteamResult SetSteamPresenceGroup(const UObject* WorldContextObject, const FString& GroupId, int32 Size);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Sets the connect string: the command line Steam gives the game when a friend clicks Join Game. An empty string removes it."))
	static FSteamResult SetSteamConnectString(const UObject* WorldContextObject, const FString& ConnectString);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Removes every rich presence key of the local user."))
	static FSteamResult ClearSteamPresence(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "One rich presence key of a friend. steam_display is the localized status text. Empty when not set."))
	static FString GetSteamFriendPresenceValue(const UObject* WorldContextObject, FSteamId Friend, FName Key);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Every rich presence key of a friend."))
	static FSteamPresenceValues GetSteamFriendPresence(const UObject* WorldContextObject, FSteamId Friend);

	UFUNCTION(BlueprintCallable, Category = "Steam|Presence", meta = (WorldContext = "WorldContextObject", ToolTip = "Asks Steam for the rich presence of a user. Friends are updated automatically; use it for users who are not friends."))
	static void RequestSteamFriendPresence(const UObject* WorldContextObject, FSteamId User);
};
