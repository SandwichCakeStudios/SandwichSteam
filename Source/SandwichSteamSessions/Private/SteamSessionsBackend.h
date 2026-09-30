// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SteamSessionTypes.h"
#include "UObject/WeakObjectPtr.h"

class IOnlineSubsystem;
class UGameInstance;
class USteamSessionsSubsystem;
class FSteamSessionsRawCallbacks;

/**
 * The engine's Steam session interface (IOnlineSession) wrapped for USteamSessionsSubsystem, plus the little raw Steamworks it needs:
 * the lobby distance filter and the "Join Game" rich presence callback.
 *
 * All methods run on the game thread. The Online Subsystem completes its delegates on the game thread, so they call the subsystem
 * directly. The raw Steam callback runs on Steam's thread and goes through the dispatcher.
 *
 * Found sessions are cached here by handle (FSteamSessionResult::Handle) because an FOnlineSessionSearchResult cannot travel through Blueprint.
 * Create with MakeShared: pending FindSessionById requests hold a shared reference.
 */
class FSteamSessionsBackend : public TSharedFromThis<FSteamSessionsBackend>
{
public:
	FSteamSessionsBackend(USteamSessionsSubsystem* InOwner, UGameInstance* InGameInstance, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);
	~FSteamSessionsBackend();

	FSteamSessionsBackend(const FSteamSessionsBackend&) = delete;
	FSteamSessionsBackend& operator=(const FSteamSessionsBackend&) = delete;

	/** The Steam session interface exists and a local user id could be resolved (a dedicated server has no user). */
	bool IsUsable() const { return Sessions.IsValid() && (LocalUserId.IsValid() || IsRunningDedicatedServer()); }

	/**
	 * Hosts NAME_GameSession from applied settings (see SteamSessionRules::ResolveSessionSettings). ProfileTag may be
	 * invalid (a session created without a profile). False when Steam refused to start the request (the completion is
	 * then not called).
	 */
	bool CreateSession(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings);

	/** Applies a settings change to the running session (visibility, slots, name, extra settings). Host / lobby owner only. */
	bool UpdateSession(const FSteamSessionSettings& Settings);

	/** The applied settings of NAME_GameSession, read back from its Online Subsystem session settings. */
	FSteamSessionSettings GetCurrentSettings() const;

	/** The session profile tag of NAME_GameSession (its OSTPROFILE lobby data), read back for a session this game only joined. Invalid without one. */
	FGameplayTag GetCurrentProfileTag() const;

	/** True when this game is the host of NAME_GameSession (not just a joined client). Only the host may update the session. */
	bool IsHost() const;

	/** Players currently in NAME_GameSession (host + joined), for Update Steam Session's "never below current players" rule. */
	int32 GetCurrentPlayerCount() const;

	/** Destroys NAME_GameSession (leaves the lobby). False when there is nothing to destroy or Steam refused. */
	bool DestroySession();

	/** Searches lobbies. Old cached results are dropped, except KeepHandle. */
	bool FindSessions(const FSteamSessionSearchOptions& Options, int32 KeepHandle);

	/** Looks up one lobby by its Steam ID (join by launch argument). */
	bool FindSessionById(FSteamId LobbyId);

	/** Joins the cached result. False when the handle is unknown or Steam refused to start the request. */
	bool JoinSession(int32 ResultHandle);

	bool HasResult(int32 ResultHandle) const { return Results.Contains(ResultHandle); }
	int32 GetCachedResultCount() const { return Results.Num(); }

	/** True while NAME_GameSession exists (hosting, joining or joined). */
	bool IsInSession() const;

	/** True when NAME_GameSession is the lobby with this ID. */
	bool IsInLobby(FSteamId LobbyId) const;

	/** Steam ID of the lobby of NAME_GameSession. Invalid when there is no session or it is not a lobby. */
	FSteamId GetCurrentLobbyId() const;

	/** The connect string of NAME_GameSession (host address or steam.<id>). False when there is none. */
	bool GetConnectString(FString& OutConnect) const;

	bool SendInvite(FSteamId Friend) const;

	/** Steam overlay invite dialog for NAME_GameSession. */
	bool ShowInviteOverlay() const;

	/** Multi-line description of the current session for Steam.Sessions.Dump. */
	FString DescribeSession() const;

	FSteamId GetLocalSteamId() const;

private:
	friend class FSteamSessionsRawCallbacks;

	/** Client calls need the interface and a user id. */
	bool HasLocalUser() const { return Sessions.IsValid() && LocalUserId.IsValid(); }

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& InviteResult);
	void OnFindSessionByIdComplete(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult);

	/** The signed in Steam user as an Online Subsystem net id (identity first, raw Steam as fallback). */
	static FUniqueNetIdPtr GetLocalUserNetId(IOnlineSubsystem& OSS);

	/** Caches the result and returns its handle-carrying description. */
	FSteamSessionResult CacheResult(const FOnlineSessionSearchResult& Source);

	TWeakObjectPtr<USteamSessionsSubsystem> Owner;
	TWeakObjectPtr<UGameInstance> GameInstance;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	IOnlineSessionPtr Sessions;
	FUniqueNetIdPtr LocalUserId;

	TSharedPtr<FOnlineSessionSearch> Search;
	TMap<int32, FOnlineSessionSearchResult> Results;
	int32 NextHandle = 1;

	FDelegateHandle CreateHandle;
	FDelegateHandle UpdateHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle InviteHandle;

	TUniquePtr<FSteamSessionsRawCallbacks> RawCallbacks;
};
