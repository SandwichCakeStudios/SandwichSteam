// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"
#include "SteamSessionsLibrary.generated.h"

/**
 * Sessions for Blueprints: invites, travel and the join request that waits for the game. Creating, finding, joining and destroying
 * sessions are the async nodes (Create / Find / Join / Destroy Steam Session). The events (On Join Requested, On Session Join Finished,
 * On Session Lost) are on the Steam Sessions Subsystem: Get Game Instance Subsystem > Steam Sessions Subsystem.
 * Every function returns a result: Steam.Error.FeatureDisabled when the Sessions feature is not available.
 */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamSessionsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the game hosts or has joined a session."))
	static bool IsInSteamSession(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the lobby of the current session (invalid when there is none). Put it in the rich presence connect string as +connect_lobby <id> so friends can Join Game."))
	static FSteamId GetSteamSessionLobbyId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Sends a Steam invite for the current session to a friend."))
	static FSteamResult SendSteamSessionInvite(const UObject* WorldContextObject, FSteamId Friend);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the Steam overlay dialog where the player picks friends to invite to the current session."))
	static FSteamResult ShowSteamInviteOverlay(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Travels the local player to the host of the current session. Only needed when Auto Travel After Join is off."))
	static FSteamResult TravelToSteamSession(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Loads a map as the host. Listen makes the game accept connections, which the host needs the first time it leaves the menu. Absolute (when already hosting) drops the current URL's options instead of keeping them.", AdvancedDisplay = "bAbsolute"))
	static FSteamResult SteamServerTravel(const UObject* WorldContextObject, const FString& MapName, bool bListen = true, bool bAbsolute = false);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "ReturnValue", ToolTip = "The join request that waits for the game (its Action is Ask Game). Returns false when none waits."))
	static bool GetPendingSteamJoinRequest(const UObject* WorldContextObject, FSteamJoinRequest& OutRequest);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Joins the waiting request, leaving the current session first when there is one."))
	static FSteamResult AcceptSteamJoinRequest(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Drops the waiting join request."))
	static void DeclineSteamJoinRequest(const UObject* WorldContextObject);

	// ---- Session settings (Phase 9c) ----

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Session", ExpandBoolAsExecs = "ReturnValue", ToolTip = "Fills Out Settings with a session profile's own presets (Default Players, Default Visibility, Fixed and Player Settings): a starting point for a host menu. Returns false when the tag is not a session profile."))
	static bool MakeSteamSessionSettingsFromProfile(const UObject* WorldContextObject, FGameplayTag ProfileTag, FSteamSessionSettings& OutSettings);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (WorldContext = "WorldContextObject", Categories = "Steam.Session", ExpandBoolAsExecs = "ReturnValue", ToolTip = "Fills Out Profile with a copy of a session profile row of the Steam App Definition (the guardrails a host menu should build its widgets from: Min/Max/Default Players, Allowed Visibilities, ...). Returns false when the tag is not a session profile."))
	static bool GetSteamSessionProfile(const UObject* WorldContextObject, FGameplayTag ProfileTag, FSteamSessionProfileDef& OutProfile);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "ReturnValue", ToolTip = "Fills Out Settings and Out Profile Tag with the settings this game applied while hosting the current session or lobby only lobby. Returns false when there is no current session or lobby."))
	static bool GetCurrentSteamSessionSettings(const UObject* WorldContextObject, FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag);

	// ---- Lobby extras (Phase 9b). Events (chat, members, data, owner, kicked) are on the Steam Sessions Subsystem. ----

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the lobby of the session or of the lobby only lobby. Invalid when there is none."))
	static FSteamId GetActiveSteamLobbyId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "True while a lobby only lobby exists (a lobby without a net session)."))
	static bool IsInSteamLobbyOnlyMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the local player owns the lobby. Only the owner can set lobby data, close the lobby and kick."))
	static bool IsSteamLobbyOwner(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the lobby owner."))
	static FSteamId GetSteamLobbyOwner(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "The members of the active lobby with name, owner and ready state. Read from Steam when called."))
	static TArray<FSteamLobbyMember> GetSteamLobbyMembers(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the lobby has members and every one is ready."))
	static bool AreAllSteamLobbyMembersReady(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Reads one lobby data value. Found is false (and the value empty) when the key is not set."))
	static FString GetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, bool& bFound);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "All lobby data as key / value pairs (includes the profile tag OSTPROFILE and the kick markers)."))
	static TMap<FString, FString> GetAllSteamLobbyData(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Owner only. Sets a lobby data value everybody in the lobby can read (map, mode, ...). Key up to 255 bytes, value up to 8192 bytes. Everybody gets On Lobby Data Changed."))
	static FSteamResult SetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Owner only. Removes a lobby data value."))
	static FSteamResult RemoveSteamLobbyData(const UObject* WorldContextObject, const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Owner only. A closed lobby takes no new members. Close it when the match starts."))
	static FSteamResult SetSteamLobbyJoinable(const UObject* WorldContextObject, bool bJoinable);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Sets a value on the local player's member data (character, team, ...). Everybody gets On Lobby Member Data Changed."))
	static FSteamResult SetSteamLobbyMemberData(const UObject* WorldContextObject, const FString& Key, const FString& Value);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Reads a value of a member's data. Empty when not set. Steam cannot list the keys of member data."))
	static FString GetSteamLobbyMemberData(const UObject* WorldContextObject, FSteamId Member, const FString& Key);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Sets the local player's ready state (member data 'ready' = 1 or 0)."))
	static FSteamResult SetSteamLobbyReady(const UObject* WorldContextObject, bool bReady);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Sends a chat message to everybody in the lobby (up to 4096 bytes). It comes back to you through On Lobby Chat Received."))
	static FSteamResult SendSteamLobbyChatMessage(const UObject* WorldContextObject, const FString& Text);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Owner only. Kicks a player by the lobby data convention (Steam has no hard kick): the player's game leaves the lobby when it sees the marker. The marker stays, so the player is kicked again when rejoining, until Forgive Steam Lobby Member. Only games using this plugin obey it."))
	static FSteamResult KickSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member, const FString& Reason);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Owner only. Removes the kick marker of a player so the player may join again."))
	static FSteamResult ForgiveSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Leaves the lobby only lobby. A session's lobby is left with Destroy Steam Session. Succeeds at once when there is no lobby only lobby."))
	static FSteamResult LeaveSteamLobby(const UObject* WorldContextObject);
};
