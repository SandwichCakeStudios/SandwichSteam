// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "SteamFriendsTypes.generated.h"

/** Online status of a Steam user. The values match Steam's EPersonaState. */
UENUM(BlueprintType)
enum class ESteamPersonaState : uint8
{
	Offline = 0,
	Online = 1,
	Busy = 2,
	Away = 3,
	Snooze = 4,
	LookingToTrade = 5,
	LookingToPlay = 6,
	Invisible = 7
};

/** Which list of users to read. */
UENUM(BlueprintType)
enum class ESteamFriendSource : uint8
{
	/** The friends of the local user. */
	Friends,
	/** Users the local user blocked. */
	Blocked,
	/** Users the local user recently played with in this game (or another). */
	RecentPlayers
};

/** Narrows a list of users. */
UENUM(BlueprintType)
enum class ESteamFriendFilter : uint8
{
	All,
	/** Everybody who is not offline. */
	Online,
	/** Playing any game. */
	InGame,
	/** Playing this game. */
	InThisGame
};

/** What changed about a friend. Delivered as a bit mask (see Has Steam Friend Change). */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ESteamFriendChange : uint8
{
	None = 0,
	/** The persona name changed. */
	Name = 1 << 0,
	/** Online status changed (came online, went offline, away, busy, ...). */
	State = 1 << 1,
	/** Started or stopped a game, or joined a server or lobby. */
	Game = 1 << 2,
	/** The avatar changed. Request it again with the User feature. */
	Avatar = 1 << 3,
	/** Friend added, removed or blocked. */
	Relationship = 1 << 4
};
ENUM_CLASS_FLAGS(ESteamFriendChange);

/** One user of a friends list. Avatars are lazy: ask the User feature with the Steam ID when a widget needs one. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMFRIENDS_API FSteamFriendInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the user."))
	FSteamId SteamId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Persona name. Empty or a placeholder while Info Loaded is false."))
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Online status."))
	ESteamPersonaState PersonaState = ESteamPersonaState::Offline;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True while the user plays a game."))
	bool bInGame = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "App ID of the game the user plays. 0 when not in a game."))
	int32 GameAppId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True while the user plays this game."))
	bool bInThisGame = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Lobby the user is in (this game only). Invalid when none or unknown."))
	FSteamId LobbyId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "False while Steam has not delivered the user's data yet. A later change event fills it in."))
	bool bInfoLoaded = true;
};

/** A friend group (Steam calls them tags) of the local user. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMFRIENDS_API FSteamFriendGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam's ID of the group."))
	int32 GroupId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Name of the group."))
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Members of the group."))
	TArray<FSteamId> Members;
};

/** Called when a friend was added, removed or blocked. Coalesced: one call per frame at most. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamFriendsListChanged);

/** Called when a friend changed. ChangeFlags is a mask of ESteamFriendChange. Coalesced: one call per friend and frame at most. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamFriendStateChanged, const FSteamFriendInfo&, Friend, int32, ChangeFlags);

/** C++ completion of a friends list read. Runs on the game thread, never after the feature shut down. */
DECLARE_DELEGATE_TwoParams(FSteamReadFriendsDelegate, const FSteamResult& /*Result*/, const TArray<FSteamFriendInfo>& /*Friends*/);
