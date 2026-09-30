// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Data/SteamAppDefinition.h"
#include "Engine/EngineBaseTypes.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"
#include "SteamSessionsSubsystem.generated.h"

class FSteamSessionsBackend;
class FSteamSessionsRawCallbacks;
class FSteamLobbyBackend;
class FSteamLobbyRaw;
class FSteamCallbackDispatcher;
class UNetDriver;
class AGameModeBase;

/**
 * Steam sessions: host, find and join matches, invites, and the travel that follows. Both client and dedicated server
 * (a dedicated server only hosts; finding, joining, invites and client travel are client features and fail with Steam.Error.WrongScope there).
 *
 * The engine's Steam Online Subsystem does the session work (IOnlineSession: a session is a Steam lobby, or a game server
 * session for dedicated servers). This feature adds what the Online Subsystem lacks: sessions by tag from the Steam App Definition
 * (profiles), one place that routes every "join" (invite, Join Game, launch arguments, cold start) through a policy, and
 * failure handling that ends the session and reports why.
 *
 * One session at a time: NAME_GameSession. One create / join / destroy runs at a time; another request meanwhile fails with
 * Steam.Error.Failed. Completion delegates run on the game thread and are not called when the feature shuts down first
 * (the Blueprint async nodes report Steam.Error.Cancelled instead).
 *
 * Networking needs the engine's SteamSockets plugin and the net driver entries in DefaultEngine.ini (Tools > Sandwich Steam > Configure Steam).
 *
 * Lobby extras (client only): the lobby of the current session (or a lobby only lobby, see Create Lobby) carries lobby data, member data
 * (ready state), chat, a member list, owner migration events and a kick convention. They work through raw ISteamMatchmaking on the
 * lobby the Online Subsystem created, so a session lobby can be used for chat and a ready check before anyone travels.
 * Lobby only mode is a lobby without a net session, for games that do not use Unreal networking or want a staging lobby: it is
 * exclusive with a session (leave one before the other).
 */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamSessionsSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Sessions subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamSessionsSubsystem* Get(const UObject* WorldContext);

	USteamSessionsSubsystem();
	virtual ~USteamSessionsSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::Both; }
	//~ End USteamFeatureSubsystem

	/** Hosts a session of a profile of the App Definition, using the profile's own presets. OnComplete runs when Steam answered. */
	FSteamResult CreateSession(const FGameplayTag& ProfileTag, FSteamSessionOpDelegate OnComplete);

	/** Hosts a session from a plain settings struct: full player control, no App Definition row and no rules. */
	FSteamResult CreateSession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);

	/** Hosts a session of a profile with player-chosen settings. The profile's optional rules clamp or refuse them; OnComplete's Applied is what Steam actually used. */
	FSteamResult CreateSession(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);

	/**
	 * Changes the settings of the running session (visibility, slots, name, extra settings). Host only (Steam.Error.NotOwner
	 * otherwise). Checked with the same rules as creating from a profile, plus: Max Players cannot drop below the players
	 * already in, and Uses Presence cannot change.
	 */
	FSteamResult UpdateSession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);

	/** The settings this game applied while hosting the current session (or lobby only lobby), and the profile it was created from (invalid without one). */
	FSteamResult GetCurrentSettings(FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag) const;

	/** The preset settings of a session profile (for the host menu), with its Fixed and Player Settings filled in. */
	FSteamResult MakeSettingsFromProfile(const FGameplayTag& ProfileTag, FSteamSessionSettings& OutSettings) const;

	/** Copy of a session profile row of the App Definition (the rules for building a host menu). */
	FSteamResult GetProfile(const FGameplayTag& ProfileTag, FSteamSessionProfileDef& OutProfile) const;

	/** Searches sessions (lobbies). Client only. */
	FSteamResult FindSessions(const FSteamSessionSearchOptions& Options, FSteamSessionFindDelegate OnComplete);

	/** Joins a found session. On success the connect string is passed on and, with Auto Travel After Join, the player travels. Client only. */
	FSteamResult JoinSession(const FSteamSessionResult& Result, FSteamSessionJoinDelegate OnComplete);

	/** Leaves or closes the session. When there is none, OnComplete runs at once with success. */
	FSteamResult DestroySession(FSteamSessionOpDelegate OnComplete);

	/** Invites a friend to the current session. Client only. */
	FSteamResult SendInvite(FSteamId Friend);

	/** Opens the Steam overlay's invite dialog for the current session. Client only. */
	FSteamResult ShowInviteOverlay();

	/** Travels the local player to the host of the current session (ClientTravel). Client only. */
	FSteamResult TravelToSession();

	/** Loads a map as the host. With bListen the game listens for connections (needed the first time a host leaves the menu). */
	FSteamResult ServerTravel(const FString& MapName, bool bListen);

	/** True while a session exists (hosting, joining or joined). */
	bool IsInSession() const;

	/** Steam ID of the lobby of the current session. Invalid when there is none. Set the rich presence connect string to "+connect_lobby <id>" so friends can Join Game. */
	FSteamId GetLobbyId() const;

	/** The join request that waits for the game (Action is Ask Game). False when there is none. */
	bool GetPendingJoinRequest(FSteamJoinRequest& OutRequest) const;

	/** Joins the waiting request (leaving the current session first when needed). */
	FSteamResult AcceptJoinRequest();

	/** Drops the waiting request. */
	void DeclineJoinRequest();

	/** The App Definition the profiles are resolved with. Null when none is assigned. */
	const USteamAppDefinition* GetDefinition() const { return Definition; }

	// ---- Lobby extras (Phase 9b). Client only. "Active lobby" = the lobby of the session, or the lobby only lobby. ----

	/** Creates a lobby without a net session, from a session profile's own presets (slots, visibility, settings). Exclusive with a session. */
	FSteamResult CreateLobby(const FGameplayTag& ProfileTag, FSteamLobbyOpDelegate OnComplete);

	/** Creates a lobby without a net session, from a plain settings struct: full player control, no profile and no rules. Exclusive with a session. */
	FSteamResult CreateLobby(const FSteamSessionSettings& Settings, FSteamLobbySettingsOpDelegate OnComplete);

	/** Creates a lobby without a net session, from a profile with player-chosen settings (the profile's optional rules apply). Exclusive with a session. */
	FSteamResult CreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamLobbySettingsOpDelegate OnComplete);

	/** Searches lobby only lobbies (raw lobby list). Lobbies of net sessions made by the Online Subsystem show up as well; find those with Find Sessions. */
	FSteamResult FindLobbies(const FSteamSessionSearchOptions& Options, FSteamLobbyFindDelegate OnComplete);

	/** Joins a lobby by its Steam ID (lobby only mode). Fails with Lobby.Full, Lobby.NotFound or Lobby.Denied. */
	FSteamResult JoinLobby(FSteamId LobbyId, FSteamLobbyOpDelegate OnComplete);

	/** Leaves the lobby only lobby. A session lobby is left with Destroy Session. Succeeds at once when there is no lobby only lobby. */
	FSteamResult LeaveLobby();

	/** True while a lobby only lobby exists. */
	bool IsInLobbyOnlyMode() const { return LobbyOnlyId.IsValid(); }

	/** Steam ID of the lobby of the session or the lobby only lobby. Invalid when there is none. */
	FSteamId GetActiveLobbyId() const;

	FSteamId GetLobbyOwnerId() const;
	bool IsLobbyOwner() const;
	int32 GetLobbyMemberLimit() const;

	/** Members of the active lobby, read from Steam when called (no polling, no cache). */
	TArray<FSteamLobbyMember> GetLobbyMembers() const;

	bool GetLobbyData(const FString& Key, FString& OutValue) const;
	TMap<FString, FString> GetAllLobbyData() const;

	/** Owner only (Steam.Error.NotOwner otherwise). Keys 'OSTPROFILE' and 'kick_*' belong to the plugin. */
	FSteamResult SetLobbyData(const FString& Key, const FString& Value);
	FSteamResult RemoveLobbyData(const FString& Key);

	/** Owner only. A closed lobby takes no new members. Use it when the match starts. */
	FSteamResult SetLobbyJoinable(bool bJoinable);

	/** Data of the local member. Others read it with GetMemberData. */
	FSteamResult SetMemberData(const FString& Key, const FString& Value);
	FString GetMemberData(FSteamId Member, const FString& Key) const;

	/** The ready convention: member data 'ready' = 1 / 0. */
	FSteamResult SetReady(bool bReady);

	/** True when the lobby has at least one member and every member is ready. */
	bool AreAllMembersReady() const;

	FSteamResult SendChatMessage(const FString& Text);

	/**
	 * Owner only. Steam has no hard kick for lobbies, so this is a convention: the owner writes the lobby data key 'kick_<SteamID64>'
	 * and the kicked client leaves when it sees it (On Lobby Kicked; a session ends with Session.Kicked). In a session the host also
	 * kicks the player from the game. The key stays until Forgive Member, so a kicked player who rejoins is kicked again (a ban for
	 * the life of the lobby). Only clients of this plugin obey it.
	 */
	FSteamResult KickMember(FSteamId Member, const FString& Reason);

	/** Owner only. Removes the kick marker so the player may join again. */
	FSteamResult ForgiveMember(FSteamId Member);

	/** Called for every chat message of the active lobby, including the local player's own. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a chat message arrives in the active lobby (also your own). Send with Send Steam Lobby Chat Message."))
	FOnSteamLobbyChatReceived OnLobbyChatReceived;

	/** Called when somebody enters or leaves the active lobby. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a member enters, leaves, disconnects or is removed by Steam. Read the whole list with Get Steam Lobby Members."))
	FOnSteamLobbyMemberChanged OnLobbyMemberChanged;

	/** Called when a member's data changed (for example the ready state). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a member changed its member data, for example the ready state."))
	FOnSteamLobbyMemberDataChanged OnLobbyMemberDataChanged;

	/** Called when the lobby data changed. Steam does not say which key: read the ones you use. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the lobby data changed. Steam does not say which key changed: read the keys you use."))
	FOnSteamLobbyDataChanged OnLobbyDataChanged;

	/** Called when Steam gave the lobby to another member because the owner left. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the lobby owner changed (the owner left and Steam picked another member). For a net session the game host does not move with it."))
	FOnSteamLobbyOwnerChanged OnLobbyOwnerChanged;

	/** Called when the owner kicked the local player. The lobby was left. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the lobby owner kicked the local player. The lobby was left (a session also reports On Session Lost with Session.Kicked)."))
	FOnSteamLobbyKicked OnLobbyKicked;

	/** Called for every join request from Steam (invite, Join Game, launch), including the ones that are ignored. Action says what the plugin does with it. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when Steam asks the game to join a session (invite, Join Game, launch). Action tells what the plugin does: join now, leave and join, wait for Accept Join Request, or ignore."))
	FOnSteamJoinRequested OnJoinRequested;

	/** Called whenever a join finished, whether it was started by the game or by a request from Steam. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a join finished, by the game or by a request from Steam. On success the connect string is set."))
	FOnSteamSessionJoinFinished OnSessionJoinFinished;

	/** Called when the session ended without the game asking (host left, timeout, kicked, failure). The session is destroyed. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the session ended by itself: Steam.Error.Session.HostLeft, Timeout, Kicked or Steam.Error.Failed. The lobby is left; return the player to the menu."))
	FOnSteamSessionLost OnSessionLost;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Sessions.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamSessionsBackend;
	friend class FSteamSessionsRawCallbacks;
	friend class FSteamLobbyRaw;

	enum class EBusyOp : uint8
	{
		None,
		Creating,
		Updating,
		Joining,
		Destroying,
		CreatingLobby,
		JoiningLobby
	};

	/** Shared body of every CreateSession overload: resolves Settings against Profile (may be null) and starts the backend request. */
	FSteamResult StartCreateSession(const FGameplayTag& ProfileTag, const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Requested, FSteamSessionSettingsOpDelegate OnComplete);

	/** Shared body of every CreateLobby overload. */
	FSteamResult StartCreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Requested, FSteamLobbySettingsOpDelegate OnComplete);

	// Game thread, called by the backend.
	void HandleCreateComplete(bool bSuccess);
	void HandleUpdateComplete(bool bSuccess);
	void HandleDestroyComplete(bool bSuccess);
	void HandleFindComplete(bool bSuccess, const TArray<FSteamSessionResult>& Found);
	void HandleJoinComplete(const FSteamResult& Result, const FString& ConnectString);
	void HandleInviteAccepted(bool bSuccess, const FSteamSessionResult& Result);
	void HandleFindByIdComplete(bool bSuccess, const FSteamSessionResult& Result);
	void HandleConnectRequest(const FSteamJoinIntent& Intent, FSteamId Friend);

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandleSessionLost(const FGameplayTag& Reason, const FString& Message);

	/** Fired after a game mode finishes InitGame (also after every travel): re-applies Max Players to its GameSession. [verify] */
	void HandleGameModeInitialized(AGameModeBase* GameMode);
	/** Sets AGameSession::MaxPlayers of GameMode (or, if null, the current world's game mode) to the hosted Max Players. No-op when not hosting or the setting is off. */
	void ApplyMaxPlayersToGameSession(AGameModeBase* GameMode = nullptr) const;

	/** Decides what to do with a request, tells the game and starts it when the decision is to join. */
	void ProcessRequest(FSteamJoinRequest Request);
	void ExecuteRequest(const FSteamJoinRequest& Request);
	void BeginLobbyLookup(FSteamJoinRequest Request);
	void RouteLaunchIntent();
	void ScheduleLaunchIntent();

	// Lobby extras. Game thread, called by the lobby backend's Steam callbacks (through the dispatcher) and call results.
	void HandleLobbyDataUpdate(FSteamId Lobby, FSteamId Member, bool bSuccess);
	void HandleLobbyChatUpdate(FSteamId Lobby, FSteamId Changed, uint32 Flags);
	void HandleLobbyChatMessage(FSteamId Lobby, FSteamId Sender, const FString& Text);
	void HandleLobbyEnterComplete(const FSteamResult& Result, FSteamId Lobby, bool bWasCreate);
	void HandleLobbyListComplete(const FSteamResult& Result, TArray<FSteamLobbyInfo>&& Lobbies);

	/** The active lobby and the lobby backend exist and the feature is a client. Fills OutLobby. */
	FSteamResult RequireLobby(FSteamId& OutLobby) const;
	/** Same, and the local user owns the lobby (Steam.Error.NotOwner otherwise). */
	FSteamResult RequireLobbyOwner(FSteamId& OutLobby) const;

	FSteamLobbyMember MakeLobbyMember(FSteamId Lobby, FSteamId Member, FSteamId Owner) const;
	/** Notices a changed lobby or owner. Broadcasts On Lobby Owner Changed when the lobby stayed the same and the owner did not. */
	void RefreshLobbyOwner();
	/** Reads the kick marker of the local player. Leaves the lobby and returns true when it is there. */
	bool CheckKicked();
	bool KickFromGameSession(FSteamId Member, const FText& Reason);

	FSteamResult StartJoin(int32 ResultHandle, FSteamSessionJoinDelegate OnComplete);
	FSteamResult ClientTravelTo(const FString& Url);
	FSteamResult RequireClient() const;
	int32 GetProtectedHandle() const;

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	TSharedPtr<FSteamSessionsBackend> Backend;
	TSharedPtr<FSteamLobbyBackend> LobbyBackend;
	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	/** The lobby only lobby (no net session). Invalid outside lobby only mode. */
	FSteamId LobbyOnlyId;

	/** The lobby and owner last seen, to notice an owner change. */
	FSteamId TrackedLobby;
	FSteamId TrackedOwner;

	FSteamLobbyOpDelegate PendingLobbyJoin;
	FSteamLobbySettingsOpDelegate PendingLobbyCreate;
	FSteamLobbyFindDelegate PendingLobbyFind;
	bool bFindingLobbies = false;
	int32 ChatReceivedCount = 0;

	EBusyOp Busy = EBusyOp::None;
	bool bFinding = false;
	FSteamSessionSettingsOpDelegate PendingCreate;
	FSteamSessionSettingsOpDelegate PendingUpdate;
	FSteamSessionOpDelegate PendingDestroy;
	FSteamSessionJoinDelegate PendingJoin;
	FSteamSessionFindDelegate PendingFind;

	/** Profile of the session (or lobby only lobby) this game hosts (for the report and Get Current Steam Session Settings). Invalid without a profile. */
	FGameplayTag ActiveProfile;

	/** Settings this game applied while hosting (session or lobby only). Only meaningful while hosting; reset on destroy / leave / a failed create. */
	FSteamSessionSettings HostedSettings;

	/** The request waiting for the game (Ask Game), and the one whose lobby is being looked up. */
	TOptional<FSteamJoinRequest> PendingRequest;
	TOptional<FSteamJoinRequest> LookupRequest;

	FSteamId LastRequestLobby;
	double LastRequestTime = 0.0;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle GameModeInitializedHandle;
	FTSTicker::FDelegateHandle LaunchIntentHandle;
	double LaunchIntentDeadline = 0.0;

	FString LastConnectString;
	int32 RequestCount = 0;
	int32 DuplicateCount = 0;
};
