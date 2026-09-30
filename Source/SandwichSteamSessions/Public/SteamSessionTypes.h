// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Data/SteamAppDefinition.h"
#include "GameplayTagContainer.h"
#include "SteamSessionTypes.generated.h"

/** Where a request to join somebody's session came from. */
UENUM(BlueprintType)
enum class ESteamJoinSource : uint8
{
	/** The player accepted an invite (Steam overlay, friends list or a toast) while the game was running. */
	Invite,
	/** The player clicked "Join Game" on a friend (rich presence connect string) while the game was running. */
	Overlay,
	/** Steam started the game to join (+connect_lobby / +connect on the command line). */
	LaunchArg
};

/** What to do with a join request while the player is already in a session. */
UENUM(BlueprintType)
enum class ESteamJoinInMatchPolicy : uint8
{
	/** Leave the current session and join the new one. */
	AutoLeave,
	/** Tell the game (On Join Requested with In Match set) and wait for Accept / Decline Join Request. */
	AskGame,
	/** Drop the request. */
	Ignore
};

/** What the plugin decided to do with a join request. */
UENUM(BlueprintType)
enum class ESteamJoinAction : uint8
{
	/** Join right away. */
	JoinNow,
	/** Leave the current session, then join. */
	LeaveAndJoin,
	/** Waiting for the game: call Accept Join Request or Decline Join Request. */
	AskGame,
	/** Dropped (already in that session, or the policy says ignore). */
	Ignore
};

/** How far from the player lobbies are searched (Steam's lobby distance filter). */
UENUM(BlueprintType)
enum class ESteamSessionDistance : uint8
{
	/** Same region, or close. */
	Close,
	/** Steam's default: a bit further than close. */
	Default,
	/** Far away as well. */
	Far,
	/** No limit. */
	Worldwide
};

/**
 * Everything one session needs: the player-chosen name, size and visibility, plus extra settings. Used both without a
 * session profile (full control) and with one (Make Steam Session Settings From Profile fills in the profile's presets;
 * the profile's optional rules then clamp or refuse what is requested). Get Applied Settings after Create / Update Steam
 * Session shows what Steam actually used.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamSessionSettings
{
	GENERATED_BODY()

	/** What players see in the browser. Empty uses the host's Steam name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "What players see in the session browser. Empty uses the host's Steam name. Trimmed and capped (a session profile's Max Name Length, or 64 characters without one)."))
	FString DisplayName;

	/** 0 = the profile's default (or 4 without a profile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ClampMin = "0", ClampMax = "250", ToolTip = "Player slots, including the host. 0 uses the session profile's default player count (or 4 without a profile). Clamped to the profile's Min/Max Players (1 to 250 without a profile); the applied value is reported back."))
	int32 MaxPlayers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Who can find and join the session. A session profile can limit which visibilities the player may pick; a disallowed choice fails the request."))
	ESteamSessionVisibility Visibility = ESteamSessionVisibility::Public;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Allow players to join after the match has started. A session profile can lock this to its own default."))
	bool bAllowJoinInProgress = true;

	/** Ignored when created from a profile: the profile decides. The applied value is reported back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Creates the session as a Steam lobby with presence, so friends see the game as joinable and invites work. Ignored when a session profile is used (designer only, the profile decides); the applied value is reported back."))
	bool bUsesPresence = true;

	/** Extra key / value pairs. A session profile can fix some keys and limit which extra ones the player may add. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Extra key and value pairs stored with the session. A session profile can fix some keys (the player cannot override them) and limit which other keys the player may add. Keys are at most 255 bytes, values at most 8192 (UTF-8). The reserved keys OSTPROFILE, OSTNAME and kick_* are refused."))
	TMap<FName, FString> Settings;
};

/** Options of a session search. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamSessionSearchOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ClampMin = "1", ClampMax = "50", ToolTip = "Most results returned."))
	int32 MaxResults = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "How far away lobbies may be."))
	ESteamSessionDistance Distance = ESteamSessionDistance::Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (Categories = "Steam.Session", ToolTip = "Only find sessions created from this profile. Leave empty to find every profile."))
	FGameplayTag ProfileTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Only find sessions whose setting (key) has exactly this value. Use the keys of the session profiles."))
	TMap<FName, FString> Filters;

	/** Steam's own filters only compare whole values, so this is applied after Steam answers: it can return fewer than MaxResults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Only keep sessions whose Display Name contains this text (case-insensitive). Applied after Steam answers (Steam's own filters only compare whole values), so this search can return fewer than Max Results."))
	FString NameContains;
};

/**
 * One found session. Pass it back to Join Steam Session. It stays valid until the next search; a stale result
 * fails with Steam.Error.Lobby.NotFound.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamSessionResult
{
	GENERATED_BODY()

	/** Identifies the result inside the subsystem. 0 is invalid. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Identifies this result inside the Sessions feature. Do not change it."))
	int32 Handle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the host (the lobby owner)."))
	FSteamId OwnerId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam name of the host."))
	FString OwnerName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the lobby, when the session is a Steam lobby."))
	FSteamId LobbyId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "What the host named the session (its Display Name). Empty when the host did not set one."))
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Session", ToolTip = "Session profile this session was created from. Invalid when it was created without a profile."))
	FGameplayTag ProfileTag;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Ping to the host in milliseconds. -1 when unknown."))
	int32 PingMs = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Player slots of the session."))
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Slots that are free."))
	int32 OpenSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "The session settings that Steam returned (profile settings, map, ...)."))
	TMap<FString, FString> Settings;

	bool IsValid() const { return Handle != 0; }
};

/** A request to join a session that came from Steam (invite, Join Game, launch). */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMSESSIONS_API FSteamJoinRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Where the request came from."))
	ESteamJoinSource Source = ESteamJoinSource::Invite;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the lobby to join. Invalid when the request is a server address."))
	FSteamId LobbyId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Server address to connect to (+connect). Empty when the request is a lobby."))
	FString ServerAddress;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the friend who invited the player. Invalid when Steam did not say."))
	FSteamId InviterId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the player was in a session when the request arrived."))
	bool bInMatch = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "What the plugin decided to do with the request."))
	ESteamJoinAction Action = ESteamJoinAction::Ignore;

	/** Result handle of the session to join (see FSteamSessionResult). 0 for server addresses. */
	int32 ResultHandle = 0;

	bool IsValid() const { return LobbyId.IsValid() || !ServerAddress.IsEmpty(); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamJoinRequested, const FSteamJoinRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamSessionJoinFinished, const FSteamResult&, Result, const FString&, ConnectString);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamSessionLost, FGameplayTag, Reason, const FString&, Message);

/** One-shot completion of a session request (game thread). Not called when the feature shuts down first. */
DECLARE_DELEGATE_OneParam(FSteamSessionOpDelegate, const FSteamResult&);
DECLARE_DELEGATE_TwoParams(FSteamSessionFindDelegate, const FSteamResult&, const TArray<FSteamSessionResult>&);
DECLARE_DELEGATE_TwoParams(FSteamSessionJoinDelegate, const FSteamResult&, const FString& /*ConnectString*/);

/** Same as FSteamSessionOpDelegate, plus the settings that were actually applied (Create With Settings, Create From Profile, Update). */
DECLARE_DELEGATE_TwoParams(FSteamSessionSettingsOpDelegate, const FSteamResult&, const FSteamSessionSettings& /*Applied*/);
