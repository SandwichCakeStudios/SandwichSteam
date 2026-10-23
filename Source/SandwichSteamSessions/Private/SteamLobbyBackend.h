// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionRules.h"
#include "SteamSessionTypes.h"
#include "UObject/WeakObjectPtr.h"

class USteamSessionsSubsystem;
class FSteamLobbyRaw;
struct FSteamSessionProfileDef;

/**
 * Raw ISteamMatchmaking for the lobby extras of the Sessions feature: lobby data, member data, chat, members, and the party lobby
 * (a lobby without a net session: create, search, join, leave, invites, the published game server of a party match).
 *
 * The lobby of a net session is created by the Online Subsystem; every method here works on any lobby the local user is in,
 * so the same calls serve both. The header is SDK-free: callbacks and pending call results live in FSteamLobbyRaw (cpp).
 *
 * All methods run on the game thread. Steam's callbacks (chat, data and member updates) run on Steam's thread, copy their payload
 * and go through the dispatcher to the owning subsystem. Call results (create, join, search) complete on the game thread through
 * TSteamCallResult and are cancelled when the backend is destroyed.
 * Create with MakeShared: pending requests hold a weak reference.
 */
class FSteamLobbyBackend : public TSharedFromThis<FSteamLobbyBackend>
{
public:
	using FEnterDelegate = TFunction<void(const FSteamResult& Result, FSteamId LobbyId)>;
	using FListDelegate = TFunction<void(const FSteamResult& Result, TArray<FSteamLobbyInfo>&& Lobbies)>;

	FSteamLobbyBackend(USteamSessionsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);
	~FSteamLobbyBackend();

	FSteamLobbyBackend(const FSteamLobbyBackend&) = delete;
	FSteamLobbyBackend& operator=(const FSteamLobbyBackend&) = delete;

	/** The Steam client matchmaking interface and a local user exist. */
	bool IsUsable() const;

	FSteamId GetLocalId() const;

	// ---- Lobby only mode (Steam calls answer through the delegate on the game thread) ----

	/**
	 * Creates a lobby from applied settings (type from the visibility, slots, name, profile tag and settings as lobby
	 * data). ProfileTag may be invalid (a lobby created without a profile). False when Steam refused to start the request.
	 */
	bool CreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FEnterDelegate OnDone);

	bool JoinLobby(FSteamId LobbyId, FEnterDelegate OnDone);

	/** Lobby list search. Only lobbies with a free slot. */
	bool RequestLobbyList(const FSteamSessionSearchOptions& Options, FListDelegate OnDone);

	void LeaveLobby(FSteamId LobbyId);

	// ---- Any lobby the local user is in ----

	FSteamId GetOwner(FSteamId LobbyId) const;
	int32 GetMemberLimit(FSteamId LobbyId) const;
	void GetMemberIds(FSteamId LobbyId, TArray<FSteamId>& OutIds) const;
	FString GetMemberName(FSteamId Member) const;

	bool GetLobbyData(FSteamId LobbyId, const FString& Key, FString& OutValue) const;
	void GetAllLobbyData(FSteamId LobbyId, TMap<FString, FString>& OutData) const;

	/** Owner only (Steam refuses otherwise). */
	bool SetLobbyData(FSteamId LobbyId, const FString& Key, const FString& Value);
	bool DeleteLobbyData(FSteamId LobbyId, const FString& Key);

	void SetMemberData(FSteamId LobbyId, const FString& Key, const FString& Value);
	FString GetMemberData(FSteamId LobbyId, FSteamId Member, const FString& Key) const;

	bool SendChat(FSteamId LobbyId, const FString& Text);

	/** Owner only. A closed lobby takes no new members but keeps the current ones. */
	bool SetJoinable(FSteamId LobbyId, bool bJoinable);

	/** Owner only. Used by Update Steam Session on a lobby only lobby (a net session goes through the Online Subsystem instead). */
	bool SetLobbyType(FSteamId LobbyId, ESteamSessionVisibility Visibility);
	bool SetLobbyMemberLimit(FSteamId LobbyId, int32 MaxPlayers);

	/** Members, limit and lobby data of a lobby as a search result entry. */
	FSteamLobbyInfo DescribeLobby(FSteamId LobbyId) const;

	// ---- Party lobby (Phase 17) ----

	/** Owner only. Any lobby type, including Invisible (Set Lobby Type above only knows the session visibilities). */
	bool SetPartyLobbyType(FSteamId LobbyId, SandwichSteam::Sessions::EPartyLobbyType Type);

	/**
	 * Owner only. Publishes the game server of the lobby: Steam sends LobbyGameCreated_t to every member. Ip / Port may be 0
	 * when Server is valid (a listen host or game server reached by Steam ID), and Server may be invalid when Ip / Port are set.
	 */
	bool SetLobbyGameServer(FSteamId LobbyId, uint32 Ip, uint16 Port, FSteamId Server);

	/** The game server last published with SetLobbyGameServer. False when none was set. */
	bool GetLobbyGameServer(FSteamId LobbyId, uint32& OutIp, uint16& OutPort, FSteamId& OutServer) const;

	/** Asks Steam for the data of a lobby the user is not in. The answer is a lobby data update (Member = Lobby) through the dispatcher. */
	bool RequestLobbyData(FSteamId LobbyId);

	/** Steam invite to a lobby (works for lobbies made without the Online Subsystem). */
	bool InviteToLobby(FSteamId LobbyId, FSteamId Friend);

	/** Steam overlay invite dialog for a lobby. */
	bool ShowLobbyInviteOverlay(FSteamId LobbyId);

private:
	TWeakObjectPtr<USteamSessionsSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
	TUniquePtr<FSteamLobbyRaw> Raw;
};
