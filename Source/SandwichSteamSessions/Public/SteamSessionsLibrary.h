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
 * Sessions for Blueprints: invites, travel, the join request that waits for the game, lobby extras and the party lobby. Creating, finding, joining and destroying
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

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the lobby of the current session (invalid when there is none). For the rich presence connect string use Get Joinable Steam Lobby Id, which also covers the party."))
	static FSteamId GetSteamSessionLobbyId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Sends a Steam invite to a friend: for the party when there is one, else for the current session (Target Active). Target Party or Session picks one."))
	static FSteamResult SendSteamSessionInvite(const UObject* WorldContextObject, FSteamId Friend, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Opens the Steam overlay dialog where the player picks friends to invite: to the party when there is one, else to the current session (Target Active). Target Party or Session picks one."))
	static FSteamResult ShowSteamInviteOverlay(const UObject* WorldContextObject, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

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

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "ReturnValue", ToolTip = "Fills Out Settings and Out Profile Tag with the settings of the current session, else of the party lobby. Returns false when there is neither."))
	static bool GetCurrentSteamSessionSettings(const UObject* WorldContextObject, FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "ReturnValue", ToolTip = "Fills Out Settings and Out Profile Tag with the settings of the party lobby, also while a session runs. A member who does not own the party gets what Steam can tell (the visibility is not readable). Returns false without a party."))
	static bool GetCurrentSteamLobbySettings(const UObject* WorldContextObject, FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Settings", ToolTip = "Changes the settings of the party lobby (name, visibility, slots, extra settings), also while a session runs. Owner only. Same rules as Update Steam Session; synchronous. During a party match the new visibility applies when the match ends."))
	static FSteamResult UpdateSteamLobby(const UObject* WorldContextObject, const FSteamSessionSettings& Settings, FSteamSessionSettings& OutAppliedSettings);

	// ---- Lobby extras (Phase 9b). Events (chat, members, data, owner, kicked) are on the Steam Sessions Subsystem. ----

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the party lobby when there is one, else of the session lobby. Invalid when there is neither."))
	static FSteamId GetActiveSteamLobbyId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "True while a party lobby exists and there is no session (a lobby without a net session)."))
	static bool IsInSteamLobbyOnlyMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "True when the local player owns the lobby. Only the owner can set lobby data, close the lobby and kick. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static bool IsSteamLobbyOwner(const UObject* WorldContextObject, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Steam ID of the lobby owner. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamId GetSteamLobbyOwner(const UObject* WorldContextObject, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "The members of the active lobby with name, owner and ready state. Read from Steam when called. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static TArray<FSteamLobbyMember> GetSteamLobbyMembers(const UObject* WorldContextObject, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "True when the lobby has members and every one is ready. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static bool AreAllSteamLobbyMembersReady(const UObject* WorldContextObject, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Reads one lobby data value. Found is false (and the value empty) when the key is not set. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FString GetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, bool& bFound, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "All lobby data as key / value pairs, including the keys the plugin owns (OSTPROFILE, OSTNAME, OSTINGAME, OSTMATCH, OSTVISIBILITY, OSTPARTY and the kick_ markers). Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static TMap<FString, FString> GetAllSteamLobbyData(const UObject* WorldContextObject, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Owner only. Sets a lobby data value everybody in the lobby can read (map, mode, ...). Key up to 255 bytes, value up to 8192 bytes. The keys the plugin owns (OSTPROFILE, OSTNAME, OSTINGAME, OSTMATCH, OSTVISIBILITY, OSTPARTY, kick_*) are refused. Everybody gets On Lobby Data Changed. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult SetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, const FString& Value, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Owner only. Removes a lobby data value. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult RemoveSteamLobbyData(const UObject* WorldContextObject, const FString& Key, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Owner only. A closed lobby takes no new members. Close it when the match starts. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult SetSteamLobbyJoinable(const UObject* WorldContextObject, bool bJoinable, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Sets a value on the local player's member data (character, team, ...). Everybody gets On Lobby Member Data Changed. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult SetSteamLobbyMemberData(const UObject* WorldContextObject, const FString& Key, const FString& Value, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Reads a value of a member's data. Empty when not set. Steam cannot list the keys of member data. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FString GetSteamLobbyMemberData(const UObject* WorldContextObject, FSteamId Member, const FString& Key, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Sets the local player's ready state (member data 'ready' = 1 or 0). Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult SetSteamLobbyReady(const UObject* WorldContextObject, bool bReady, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Sends a chat message to everybody in the lobby (up to 4096 bytes). It comes back to you through On Lobby Chat Received. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult SendSteamLobbyChatMessage(const UObject* WorldContextObject, const FString& Text, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Owner only. Kicks a player by the lobby data convention (Steam has no hard kick): the player's game leaves the lobby when it sees the marker. The marker stays, so the player is kicked again when rejoining, until Forgive Steam Lobby Member. Only games using this plugin obey it. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult KickSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member, const FString& Reason, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Target", ToolTip = "Owner only. Removes the kick marker of a player so the player may join again. Target picks the lobby: Active is the party when there is one, else the session lobby."))
	static FSteamResult ForgiveSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Lobby", meta = (WorldContext = "WorldContextObject", ToolTip = "Leaves the party lobby, also while a session runs. A session's lobby is left with Destroy Steam Session. Succeeds at once when there is no party."))
	static FSteamResult LeaveSteamLobby(const UObject* WorldContextObject);

	// ---- Party lobby (Phase 17). Events (On Party Match Started / Ended, On Party Join Finished) are on the Steam Sessions Subsystem. ----

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the player is in a party lobby (Create / Join Steam Lobby). The party stays when sessions start and end."))
	static bool IsInSteamParty(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the party lobby. Invalid when there is none."))
	static FSteamId GetSteamPartyLobbyId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "The lobby friends should join: the party when there is one, else the session lobby. Set the rich presence connect string to +connect_lobby <id> with it so Join Game works for both."))
	static FSteamId GetJoinableSteamLobbyId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the party is in a match (the owner called Begin Steam Party Match and not End yet)."))
	static bool IsSteamPartyInMatch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ExpandBoolAsExecs = "ReturnValue", ToolTip = "The running party match (where the members go). Returns false when the party is not in a match."))
	static bool GetSteamPartyMatch(const UObject* WorldContextObject, FSteamPartyMatch& OutMatch);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "Party owner only. Starts a party match: members get On Party Match Started and, with Auto Follow Party Match, join the Session Lobby or travel to the Server Steam Id / Address. Call it when the host accepts connections (after Steam Server Travel with Listen). The party's lobby type changes to Party Match Visibility until End Steam Party Match."))
	static FSteamResult BeginSteamPartyMatch(const UObject* WorldContextObject, const FSteamPartyMatch& Match);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "Party owner only. Ends the party match: members get On Party Match Ended and the party's lobby type and joinable flag are restored. With Auto End Party Match this happens by itself when the host's session ends."))
	static FSteamResult EndSteamPartyMatch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "Joins the running party match. Needed when Auto Follow Party Match is off, or to follow a match after joining the party late."))
	static FSteamResult FollowSteamPartyMatch(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|Party", meta = (WorldContext = "WorldContextObject", ToolTip = "Leaves the party lobby (same as Leave Steam Lobby). A running session is not touched."))
	static FSteamResult LeaveSteamParty(const UObject* WorldContextObject);
};
