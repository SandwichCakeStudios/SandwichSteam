// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCoreSubsystem.h"
#include "Core/SteamId.h"
#include "Engine/EngineBaseTypes.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"

/**
 * Pure session rules (no engine subsystems, no Steam): how a profile becomes session flags, what to do with a join request,
 * how a network failure is classified and how a rich presence connect string is read. Kept apart so they can be unit tested.
 */
namespace SandwichSteam::Sessions
{
	/** The Online Subsystem session flags a profile turns into. */
	struct FSessionShape
	{
		int32 PublicConnections = 0;
		int32 PrivateConnections = 0;
		bool bShouldAdvertise = false;
		bool bAllowJoinViaPresence = false;
		bool bAllowJoinViaPresenceFriendsOnly = false;
		bool bAllowInvites = true;
		bool bUsesPresence = false;
		bool bUseLobbies = false;
		bool bAllowJoinInProgress = true;
	};

	FSessionShape MakeShape(ESteamSessionVisibility Visibility, int32 MaxPlayers, bool bUsesPresence, bool bAllowJoinInProgress);

	/** What to do with a join request. Same session as the current one is always ignored. */
	ESteamJoinAction DecideJoin(ESteamJoinInMatchPolicy Policy, bool bInMatch, bool bAutoJoin, bool bAlreadyInTarget);

	/** True when a request for the same lobby was already handled within the window (Steam and the OSS can both report one invite). */
	bool IsDuplicateRequest(double Now, double LastTime, const FSteamId& LastLobby, const FSteamId& Lobby, float WindowSeconds);

	/** Why a session ended. */
	enum class ELostReason : uint8
	{
		HostLeft,
		Timeout,
		Kicked,
		Failed
	};

	ELostReason ClassifyNetworkFailure(ENetworkFailure::Type Failure);
	const TCHAR* LexToString(ELostReason Reason);

	/**
	 * Reads "+connect_lobby <id>" or "+connect <address>" from a rich presence connect string (the same words Steam puts on
	 * the command line). Returns false when the text holds neither.
	 */
	bool ParseConnectString(const FString& Connect, FSteamJoinIntent& OutIntent);

	/** Steam lobby data key that carries the profile tag of a session. */
	inline const TCHAR* ProfileKey() { return TEXT("OSTPROFILE"); }

	/** Steam lobby data key that carries the player-chosen display name of a session (Phase 9c). */
	inline const TCHAR* NameKey() { return TEXT("OSTNAME"); }

	/** Party lobby data key: 1 while a party match runs, 0 otherwise. */
	inline const TCHAR* InGameKey() { return TEXT("OSTINGAME"); }

	/** Party lobby data key: the running party match (EncodePartyMatch). */
	inline const TCHAR* MatchKey() { return TEXT("OSTMATCH"); }

	/** Party lobby data key: the lobby type and joinable flag saved by Begin Party Match (EncodeSavedLobbyState), so a new owner can restore them. */
	inline const TCHAR* VisibilityKey() { return TEXT("OSTVISIBILITY"); }

	/** Lobby data key that marks a party lobby (1). Join requests for such a lobby join the party, not a net session. */
	inline const TCHAR* PartyKey() { return TEXT("OSTPARTY"); }

	// ---- Lobby extras (Phase 9b) ----

	/** Steam limits: a lobby key is at most 255 bytes, a lobby / member data value 8192 bytes, a chat message 4096 bytes (UTF-8). */
	constexpr int32 MaxLobbyKeyBytes = 255;
	constexpr int32 MaxLobbyValueBytes = 8192;
	constexpr int32 MaxChatBytes = 4096;

	/** Member data key of the ready convention (value 1 = ready). */
	inline const TCHAR* ReadyKey() { return TEXT("ready"); }

	/** Prefix of the lobby data keys that mark a kicked member ("kick_<SteamID64>"). */
	inline const TCHAR* KickKeyPrefix() { return TEXT("kick_"); }

	FString KickKey(const FSteamId& Member);

	/** Keys the plugin owns (profile tag, session name, party keys, kick markers). The game cannot set or delete them. */
	bool IsReservedLobbyKey(const FString& Key);

	/** "1" and "true" (any case) are true. Anything else, including empty, is false. */
	bool ParseFlag(const FString& Value);

	/** Each returns false and fills OutError with the reason. */
	bool ValidateLobbyKey(const FString& Key, FText& OutError);
	bool ValidateLobbyValue(const FString& Value, FText& OutError);
	bool ValidateChatText(const FString& Text, FText& OutError);

	/** Steam's EChatMemberStateChange flags of a lobby chat update as one change (the strongest flag wins). */
	ESteamLobbyMemberChange MemberChangeFromFlags(uint32 Flags);

	/** What entering a lobby came to. */
	enum class ELobbyEnterOutcome : uint8
	{
		Success,
		NotFound,
		Denied,
		Full,
		Failed
	};

	/** Maps Steam's EChatRoomEnterResponse (1 success, 2 does not exist, 3 not allowed, 4 full, 5 error, 6 banned, ...). */
	ELobbyEnterOutcome ClassifyLobbyEnter(int32 Response);

	// ---- Session settings (Phase 9c) ----

	/** Plugin-wide limits that apply even without a session profile. */
	constexpr int32 MinSessionPlayers = 1;
	constexpr int32 MaxSessionPlayers = 250;
	constexpr int32 DefaultSessionPlayers = 4;
	constexpr int32 DefaultMaxNameLength = 64;

	/**
	 * Turns a request into what is actually applied. Without a profile only the plugin limits apply (1..250 players, name
	 * cap, reserved keys, byte limits). With a profile: Max Players 0 -> the profile's default, then clamped to Min/Max;
	 * a disallowed visibility or a join-in-progress change the profile locks fails; Uses Presence always comes from the
	 * profile; settings are Fixed + Player Settings defaults, overlaid with the request (a fixed key, or an unknown key
	 * when Allow Extra Settings is off, fails). False and OutError set on failure; OutApplied is only meaningful on success.
	 */
	bool ResolveSessionSettings(const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Requested, FSteamSessionSettings& OutApplied, FText& OutError);

	/** The preset the host menu can show for a profile: its defaults with the fixed and player settings filled in. */
	void MakeDefaultSettings(const FSteamSessionProfileDef& Profile, FSteamSessionSettings& OutSettings);

	/**
	 * Same rules as ResolveSessionSettings, plus: Max Players may not drop below CurrentPlayers, and Uses Presence may not
	 * change from Current (it decides whether the session is a Steam lobby, which cannot be flipped after creation).
	 */
	bool ValidateSessionUpdate(const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Current, const FSteamSessionSettings& Requested, int32 CurrentPlayers, FSteamSessionSettings& OutApplied, FText& OutError);

	// ---- Party lobby (Phase 17) ----

	/** The lobby a target means: Active = Party when valid, else Session. Invalid when that lobby does not exist. */
	FSteamId ResolveLobbyTarget(ESteamLobbyTarget Target, const FSteamId& Party, const FSteamId& Session);

	/** The match as one lobby data value ("lobby=<id>;server=<id>;addr=<ip:port>", empty parts left out). Empty for an invalid match. */
	FString EncodePartyMatch(const FSteamPartyMatch& Match);

	/** Reads EncodePartyMatch's text. False (and an empty match) when nothing usable is in it. */
	bool DecodePartyMatch(const FString& Text, FSteamPartyMatch& OutMatch);

	/** Steam's lobby types (ELobbyType without the SDK): what a party lobby is set to. */
	enum class EPartyLobbyType : uint8
	{
		Public,
		FriendsOnly,
		Private,
		Invisible
	};

	EPartyLobbyType ToPartyLobbyType(ESteamSessionVisibility Visibility);

	/** Lobby type and joinable flag of a party lobby. */
	struct FPartyLobbyState
	{
		EPartyLobbyType Type = EPartyLobbyType::Public;
		bool bJoinable = true;

		friend bool operator==(const FPartyLobbyState& A, const FPartyLobbyState& B) { return A.Type == B.Type && A.bJoinable == B.bJoinable; }
	};

	/** The state while a party match runs: Before with the match visibility and, when bClose, closed. */
	FPartyLobbyState MakeMatchLobbyState(const FPartyLobbyState& Before, ESteamPartyMatchVisibility Visibility, bool bClose);

	/** "<type>,<joinable>" for the VisibilityKey lobby data, and back. Decode is false on anything else. */
	FString EncodeSavedLobbyState(const FPartyLobbyState& State);
	bool DecodeSavedLobbyState(const FString& Text, FPartyLobbyState& OutState);

	/** How a member follows a party match. */
	enum class EPartyFollow : uint8
	{
		/** Nothing to do: invalid match, or the local player already hosts it / is in it. */
		None,
		/** Join the session lobby through the Online Subsystem. */
		Session,
		/** Travel to steam.<ServerSteamId>. */
		SteamId,
		/** Travel to the address. */
		Address
	};

	/** Server Steam Id first, then Session Lobby, then Address. None when the local player is the server or already in the session lobby. */
	EPartyFollow DecideFollow(const FSteamPartyMatch& Match, const FSteamId& LocalId, const FSteamId& CurrentSessionLobby);

	/** Reads "a.b.c.d:port" (IPv4, port 1..65535). False on anything else, for example a host name. */
	bool ParseIPv4Address(const FString& Address, uint32& OutIp, uint16& OutPort);
	FString FormatIPv4Address(uint32 Ip, uint16 Port);
}
