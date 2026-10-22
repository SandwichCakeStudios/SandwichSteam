// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "SteamSessionTypes.h"
#include "SteamLobbyTypes.generated.h"

/** Which lobby a lobby function works on, or which lobby an event came from. */
UENUM(BlueprintType)
enum class ESteamLobbyTarget : uint8
{
	/** The party lobby when there is one, else the lobby of the session (the behaviour before party lobbies existed). */
	Active,
	/** The party lobby (Create / Join Steam Lobby). It outlives sessions. */
	Party,
	/** The lobby the Online Subsystem made for the current session. It ends with the session. */
	Session
};

/** The lobby type of the party while a party match runs. */
UENUM(BlueprintType)
enum class ESteamPartyMatchVisibility : uint8
{
	/** Leave the lobby type as it is. */
	KeepAsIs,
	/** Only friends of members can find and join it. */
	FriendsOnly,
	/** Hidden from search and friends. Invites still work. */
	Private,
	/** Hidden, and Steam does not show it as the members' lobby. Invites still work. */
	Invisible
};

/** What happened to a lobby member. */
UENUM(BlueprintType)
enum class ESteamLobbyMemberChange : uint8
{
	/** Joined the lobby. */
	Entered,
	/** Left the lobby on purpose. */
	Left,
	/** Lost the connection to Steam. */
	Disconnected,
	/** Was kicked by Steam (not the owner kick of this plugin, which shows as On Lobby Kicked). */
	Kicked,
	/** Was banned by Steam. */
	Banned
};

/** One member of the lobby. Member data cannot be listed: read a key with Get Steam Lobby Member Data. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamLobbyMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the member."))
	FSteamId Id;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam name of the member. Can be empty until Steam knows it."))
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True for the lobby owner (the host of the lobby)."))
	bool bIsOwner = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True for the local player."))
	bool bIsLocal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "The member's ready state: its member data 'ready' is 1 (set with Set Steam Lobby Ready)."))
	bool bReady = false;
};

/** A chat message received in the lobby. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamLobbyChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Who sent the message."))
	FSteamId Sender;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam name of the sender."))
	FString SenderName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "The message."))
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the local player sent it (Steam echoes your own messages back)."))
	bool bIsLocal = false;
};

/** A lobby found by Find Steam Lobbies (party lobbies). Join it with its Lobby Id. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamLobbyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the lobby. Pass it to Join Steam Lobby."))
	FSteamId LobbyId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the lobby owner."))
	FSteamId OwnerId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "What the host named the session (its Display Name). Empty when the host did not set one."))
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Session", ToolTip = "Session profile this lobby was created from. Invalid when it was created without a profile."))
	FGameplayTag ProfileTag;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Members in the lobby."))
	int32 MemberCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Most members the lobby holds."))
	int32 MaxMembers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "The lobby data (profile settings, keys the owner set)."))
	TMap<FString, FString> Data;
};

/**
 * Where the members of a party go for a match (Begin Steam Party Match). At least one field is set; members follow the first
 * one that is set, in this order: Session Lobby, Server Steam Id, Address.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamPartyMatch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Lobby of the net session of the match; it names the match (rejoin). Members join it only when there is no Server Steam Id: the Steam Online Subsystem cannot look a lobby up by its ID. Host Steam Party Session fills in both."))
	FSteamId SessionLobby;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Steam ID of the listen host or game server. Members travel to steam.<id> (SteamSockets), no Online Subsystem session needed."))
	FSteamId ServerSteamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "ip:port of a dedicated server. Members travel to it. The plugin only publishes the address, it does not start a server."))
	FString Address;

	bool IsValid() const { return SessionLobby.IsValid() || ServerSteamId.IsValid() || !Address.IsEmpty(); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyChatReceived, const FSteamLobbyChatMessage&, Message, ESteamLobbyTarget, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamLobbyMemberChanged, const FSteamLobbyMember&, Member, ESteamLobbyMemberChange, Change, ESteamLobbyTarget, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyMemberDataChanged, const FSteamLobbyMember&, Member, ESteamLobbyTarget, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamLobbyDataChanged, ESteamLobbyTarget, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamLobbyOwnerChanged, FSteamId, NewOwner, FSteamId, PreviousOwner, ESteamLobbyTarget, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamLobbyKicked, const FString&, Reason, ESteamLobbyTarget, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamPartyMatchStarted, const FSteamPartyMatch&, Match);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamPartyMatchEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamPartyJoinFinished, const FSteamResult&, Result, FSteamId, LobbyId);

/** One-shot completion of a lobby request (game thread). Not called when the feature shuts down first. */
DECLARE_DELEGATE_TwoParams(FSteamLobbyOpDelegate, const FSteamResult&, FSteamId /*LobbyId*/);
DECLARE_DELEGATE_TwoParams(FSteamLobbyFindDelegate, const FSteamResult&, const TArray<FSteamLobbyInfo>&);

/** Same as FSteamLobbyOpDelegate, plus the settings that were actually applied (Create Lobby With Settings, Create Lobby From Profile). */
DECLARE_DELEGATE_ThreeParams(FSteamLobbySettingsOpDelegate, const FSteamResult&, FSteamId /*LobbyId*/, const FSteamSessionSettings& /*Applied*/);
