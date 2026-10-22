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
 * Lobby extras (client only): a lobby carries lobby data, member data (ready state), chat, a member list, owner migration events and a
 * kick convention. They work through raw ISteamMatchmaking, on two kinds of lobby:
 * - the session lobby, which the Online Subsystem created for the current session and which ends with it;
 * - the party lobby (Create / Join Lobby), which belongs to the plugin and outlives sessions. A party and a session may exist at the
 *   same time (one of each): the party owner starts a match (Begin Party Match, Host Party Session), members follow, and when the
 *   match ends everyone is still in the party. Without a session the party is "lobby only mode".
 * Every lobby function takes an optional Target (Active = the party when there is one, else the session lobby).
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
	 * Changes the settings of the running session (visibility, slots, name, extra settings), or of the party lobby when there is
	 * no session. Host only (Steam.Error.NotOwner otherwise). Checked with the same rules as creating from a profile, plus: Max Players
	 * cannot drop below the players already in, and Uses Presence cannot change.
	 */
	FSteamResult UpdateSession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);

	/** Same rules as UpdateSession, always on the party lobby (also while a session runs). Owner only. Synchronous: OnComplete runs before it returns. */
	FSteamResult UpdateLobby(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);

	/** The settings of the current session, else of the party lobby, and the profile it was created from (invalid without one). */
	FSteamResult GetCurrentSettings(FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag) const;

	/** The settings of the party lobby (also while a session runs). A member who does not own it gets what Steam can tell (no visibility). */
	FSteamResult GetCurrentLobbySettings(FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag) const;

	/** The preset settings of a session profile (for the host menu), with its Fixed and Player Settings filled in. */
	FSteamResult MakeSettingsFromProfile(const FGameplayTag& ProfileTag, FSteamSessionSettings& OutSettings) const;

	/** Copy of a session profile row of the App Definition (the rules for building a host menu). */
	FSteamResult GetProfile(const FGameplayTag& ProfileTag, FSteamSessionProfileDef& OutProfile) const;

	/** Searches sessions (lobbies). Client only. */
	FSteamResult FindSessions(const FSteamSessionSearchOptions& Options, FSteamSessionFindDelegate OnComplete);

	/** Joins a found session. On success the connect string is passed on and, with Auto Travel After Join, the player travels. Client only. */
	FSteamResult JoinSession(const FSteamSessionResult& Result, FSteamSessionJoinDelegate OnComplete);

	/** Leaves or closes the session. When there is none, OnComplete runs at once with success. The party lobby is not touched. */
	FSteamResult DestroySession(FSteamSessionOpDelegate OnComplete);

	/** Invites a friend to the target lobby (Active: the party when there is one, else the session). Client only. */
	FSteamResult SendInvite(FSteamId Friend, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/** Opens the Steam overlay's invite dialog for the target lobby (Active: the party when there is one, else the session). Client only. */
	FSteamResult ShowInviteOverlay(ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/** Travels the local player to the host of the current session (ClientTravel). Client only. */
	FSteamResult TravelToSession();

	/**
	 * Loads a map as the host. With bListen the game listens for connections (needed the first time a host leaves the menu).
	 * bAbsolute applies when already hosting: false keeps the current URL's options (relative travel), true starts from a clean URL.
	 * Leaving the menu (standalone) is always absolute.
	 */
	FSteamResult ServerTravel(const FString& MapName, bool bListen, bool bAbsolute = false);

	/** True while a session exists (hosting, joining or joined). */
	bool IsInSession() const;

	/** Steam ID of the lobby of the current session. Invalid when there is none. */
	FSteamId GetLobbyId() const;

	/** Same as GetLobbyId: the lobby the Online Subsystem made for the current session. */
	FSteamId GetSessionLobbyId() const { return GetLobbyId(); }

	/**
	 * The lobby friends should join: the party when there is one, else the session lobby. Set the rich presence connect string to
	 * "+connect_lobby <id>" so friends can Join Game; the plugin routes it to the party or the session.
	 */
	FSteamId GetJoinableLobbyId() const;

	/** The join request that waits for the game (Action is Ask Game). False when there is none. */
	bool GetPendingJoinRequest(FSteamJoinRequest& OutRequest) const;

	/** Joins the waiting request (leaving the current session, or the current party for a party request, first when needed). */
	FSteamResult AcceptJoinRequest();

	/** Drops the waiting request. */
	void DeclineJoinRequest();

	/** The App Definition the profiles are resolved with. Null when none is assigned. */
	const USteamAppDefinition* GetDefinition() const { return Definition; }

	// ---- Lobby extras (Phase 9b, party lobby Phase 17). Client only. ----

	/** Creates the party lobby from a session profile's own presets (slots, visibility, settings). Works with or without a session; one party at a time. */
	FSteamResult CreateLobby(const FGameplayTag& ProfileTag, FSteamLobbyOpDelegate OnComplete);

	/** Creates the party lobby from a plain settings struct: full player control, no profile and no rules. */
	FSteamResult CreateLobby(const FSteamSessionSettings& Settings, FSteamLobbySettingsOpDelegate OnComplete);

	/** Creates the party lobby from a profile with player-chosen settings (the profile's optional rules apply). */
	FSteamResult CreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamLobbySettingsOpDelegate OnComplete);

	/** Searches lobbies (raw lobby list). Lobbies of net sessions made by the Online Subsystem show up as well; find those with Find Sessions. */
	FSteamResult FindLobbies(const FSteamSessionSearchOptions& Options, FSteamLobbyFindDelegate OnComplete);

	/** Joins a lobby by its Steam ID as the party. Fails with Lobby.Full, Lobby.NotFound or Lobby.Denied, or when already in a party. */
	FSteamResult JoinLobby(FSteamId LobbyId, FSteamLobbyOpDelegate OnComplete);

	/** Leaves the party lobby, also while a session runs. A session lobby is left with Destroy Session. Succeeds at once when there is no party. */
	FSteamResult LeaveLobby();

	/** True while a party lobby exists. */
	bool IsInParty() const { return PartyLobbyId.IsValid(); }

	/** Steam ID of the party lobby. Invalid when there is none. */
	FSteamId GetPartyLobbyId() const { return PartyLobbyId; }

	/** True while a party lobby exists and there is no session ("a lobby without a net session"). */
	bool IsInLobbyOnlyMode() const;

	/** Steam ID of the party lobby, else of the session lobby. Invalid when there is none. */
	FSteamId GetActiveLobbyId() const;

	/** Steam ID of the lobby a target means. Invalid when that lobby does not exist. */
	FSteamId GetLobbyIdFor(ESteamLobbyTarget Target) const;

	FSteamId GetLobbyOwnerId(ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;
	bool IsLobbyOwner(ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;
	int32 GetLobbyMemberLimit(ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;

	/** Members of the target lobby, read from Steam when called (no polling, no cache). */
	TArray<FSteamLobbyMember> GetLobbyMembers(ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;

	bool GetLobbyData(const FString& Key, FString& OutValue, ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;
	TMap<FString, FString> GetAllLobbyData(ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;

	/** Owner only (Steam.Error.NotOwner otherwise). Keys 'OSTPROFILE', 'OSTNAME', 'OSTINGAME', 'OSTMATCH', 'OSTVISIBILITY', 'OSTPARTY' and 'kick_*' belong to the plugin. */
	FSteamResult SetLobbyData(const FString& Key, const FString& Value, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);
	FSteamResult RemoveLobbyData(const FString& Key, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/** Owner only. A closed lobby takes no new members. Use it when the match starts. */
	FSteamResult SetLobbyJoinable(bool bJoinable, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/** Data of the local member. Others read it with GetMemberData. */
	FSteamResult SetMemberData(const FString& Key, const FString& Value, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);
	FString GetMemberData(FSteamId Member, const FString& Key, ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;

	/** The ready convention: member data 'ready' = 1 / 0. */
	FSteamResult SetReady(bool bReady, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/** True when the lobby has at least one member and every member is ready. */
	bool AreAllMembersReady(ESteamLobbyTarget Target = ESteamLobbyTarget::Active) const;

	FSteamResult SendChatMessage(const FString& Text, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/**
	 * Owner only. Steam has no hard kick for lobbies, so this is a convention: the owner writes the lobby data key 'kick_<SteamID64>'
	 * and the kicked client leaves when it sees it (On Lobby Kicked; a session ends with Session.Kicked). The key stays until Forgive
	 * Member, so a kicked player who rejoins is kicked again (a ban for the life of the lobby). Only clients of this plugin obey it.
	 * The session lobby also kicks the player from the game; the party lobby only writes the marker (call it twice for both).
	 */
	FSteamResult KickMember(FSteamId Member, const FString& Reason, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	/** Owner only. Removes the kick marker so the player may join again. */
	FSteamResult ForgiveMember(FSteamId Member, ESteamLobbyTarget Target = ESteamLobbyTarget::Active);

	// ---- Party match (Phase 17) ----

	/**
	 * Party owner only. Tells the members where the match is: saves the party's lobby type and joinable flag, writes the reserved lobby
	 * data OSTINGAME / OSTMATCH, publishes the game server (SetLobbyGameServer) when Server Steam Id or an IPv4 Address is set, and
	 * applies Party Match Visibility / Close Party During Match. Members get On Party Match Started and, with Auto Follow Party Match, join.
	 */
	FSteamResult BeginPartyMatch(const FSteamPartyMatch& Match);

	/** Party owner only. Ends the party match: OSTINGAME = 0, OSTMATCH removed, lobby type and joinable flag restored. Members get On Party Match Ended. */
	FSteamResult EndPartyMatch();

	/** Creates a session (as Create Session) and, when it succeeded, calls Begin Party Match with its lobby. Party owner only. */
	FSteamResult HostPartySession(const FGameplayTag& ProfileTag, FSteamSessionOpDelegate OnComplete);
	FSteamResult HostPartySession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);
	FSteamResult HostPartySession(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete);

	/** Joins the running party match (for a game that turned Auto Follow Party Match off, or a member who joined mid-match). */
	FSteamResult FollowPartyMatch();

	/** The running party match, read from the party's lobby data. False when the party is not in a match. */
	bool GetPartyMatch(FSteamPartyMatch& OutMatch) const;

	/** True while the party is in a match (its lobby data OSTINGAME is 1). */
	bool IsPartyInMatch() const;

	/** Called for every chat message of the party or the session lobby, including the local player's own. Source says which. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a chat message arrives in the party or the session lobby (also your own). Source says which lobby. Send with Send Steam Lobby Chat Message."))
	FOnSteamLobbyChatReceived OnLobbyChatReceived;

	/** Called when somebody enters or leaves the party or the session lobby. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a member enters, leaves, disconnects or is removed by Steam. Source says which lobby. Read the whole list with Get Steam Lobby Members."))
	FOnSteamLobbyMemberChanged OnLobbyMemberChanged;

	/** Called when a member's data changed (for example the ready state). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a member changed its member data, for example the ready state. Source says which lobby."))
	FOnSteamLobbyMemberDataChanged OnLobbyMemberDataChanged;

	/** Called when the lobby data changed. Steam does not say which key: read the ones you use. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the lobby data changed. Steam does not say which key changed: read the keys you use. Source says which lobby."))
	FOnSteamLobbyDataChanged OnLobbyDataChanged;

	/** Called when Steam gave the lobby to another member because the owner left. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the lobby owner changed (the owner left and Steam picked another member). For a net session the game host does not move with it. Source says which lobby."))
	FOnSteamLobbyOwnerChanged OnLobbyOwnerChanged;

	/** Called when the owner kicked the local player. That lobby was left. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the lobby owner kicked the local player. That lobby was left: a party kick keeps the session, a session kick also reports On Session Lost with Session.Kicked and keeps the party."))
	FOnSteamLobbyKicked OnLobbyKicked;

	/** Called when the party owner started a party match (also on the owner, and when joining a party that is in a match). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|Party", meta = (ToolTip = "Called when the party owner started a party match (also on the owner, and when you join a party that is already in a match). With Auto Follow Party Match the plugin joins by itself; otherwise call Follow Steam Party Match."))
	FOnSteamPartyMatchStarted OnPartyMatchStarted;

	/** Called when the party owner ended the party match. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|Party", meta = (ToolTip = "Called when the party owner ended the party match. Everyone is still in the party: show the party screen."))
	FOnSteamPartyMatchEnded OnPartyMatchEnded;

	/** Called when a party join from an invite, Join Game or launch finished. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|Party", meta = (ToolTip = "Called when a join into a party lobby finished that came from Steam (invite, Join Game, launch). Joins the game started itself report through Join Steam Lobby."))
	FOnSteamPartyJoinFinished OnPartyJoinFinished;

	/** Called for every join request from Steam (invite, Join Game, launch), including the ones that are ignored. Action says what the plugin does with it. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when Steam asks the game to join a session or party (invite, Join Game, launch). Action tells what the plugin does: join now, leave and join, wait for Accept Join Request, or ignore. Party is true for a party lobby."))
	FOnSteamJoinRequested OnJoinRequested;

	/** Called whenever a join finished, whether it was started by the game or by a request from Steam. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when a session join finished, by the game, by a request from Steam or by following a party match. On success the connect string is set."))
	FOnSteamSessionJoinFinished OnSessionJoinFinished;

	/** Called when the session ended without the game asking (host left, timeout, kicked, failure). The session is destroyed. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called when the session ended by itself: Steam.Error.Session.HostLeft, Timeout, Kicked or Steam.Error.Failed. The session lobby is left, the party stays: return the player to the party screen when Is In Steam Party, else to the menu."))
	FOnSteamSessionLost OnSessionLost;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Sessions.Dump. */
	FString BuildDebugString() const;

	/** Steam.Sessions.ProbeLobby: the cached lobby data of any lobby (party key, owner), and a request for fresh data. */
	FString DebugProbeLobby(FSteamId Lobby);
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

	/** The lobby and owner last seen, to notice an owner change. One per lobby kind. */
	struct FTrackedLobby
	{
		FSteamId Lobby;
		FSteamId Owner;
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
	/**
	 * Finds out whether a requested lobby is a party lobby (raw RequestLobbyData, OSTPARTY) and routes it: a party joins the party;
	 * anything else is looked up as a session, unless bSessionFallback is false (a raw lobby invite: the Online Subsystem handles those).
	 */
	void BeginLobbyLookup(FSteamJoinRequest Request, bool bSessionFallback = true);
	/** The session part of the lookup: FindSessionById, then ProcessRequest. */
	void BeginSessionLookup(FSteamJoinRequest Request);
	/** Called when the lobby data of the looked-up lobby arrived, or the wait timed out. */
	void ResolvePartyLookup(bool bAnswered);
	/** Same as ProcessRequest for a party lobby: the policy uses "already in a party" as the in-match flag. */
	void ProcessPartyRequest(FSteamJoinRequest Request);
	void ExecutePartyRequest(const FSteamJoinRequest& Request);
	void RouteLaunchIntent();
	void ScheduleLaunchIntent();

	// Lobby extras. Game thread, called by the lobby backend's Steam callbacks (through the dispatcher) and call results.
	void HandleLobbyDataUpdate(FSteamId Lobby, FSteamId Member, bool bSuccess);
	void HandleLobbyChatUpdate(FSteamId Lobby, FSteamId Changed, uint32 Flags);
	void HandleLobbyChatMessage(FSteamId Lobby, FSteamId Sender, const FString& Text);
	void HandleLobbyEnterComplete(const FSteamResult& Result, FSteamId Lobby, bool bWasCreate);
	void HandleLobbyListComplete(const FSteamResult& Result, TArray<FSteamLobbyInfo>&& Lobbies);
	void HandleLobbyGameCreated(FSteamId Lobby, FSteamId Server, uint32 Ip, uint16 Port);
	void HandleLobbyJoinRequested(FSteamId Lobby, FSteamId Friend);

	/** The target lobby and the lobby backend exist and the feature is a client. Fills OutLobby. */
	FSteamResult RequireLobby(ESteamLobbyTarget Target, FSteamId& OutLobby) const;
	/** Same, and the local user owns the lobby (Steam.Error.NotOwner otherwise). */
	FSteamResult RequireLobbyOwner(ESteamLobbyTarget Target, FSteamId& OutLobby) const;

	/** True when Lobby is the party or the session lobby (the only lobbies whose events are passed on). */
	bool IsKnownLobby(FSteamId Lobby) const;
	/** Party when Lobby is the party lobby, else Session. */
	ESteamLobbyTarget SourceOf(FSteamId Lobby) const;

	FSteamLobbyMember MakeLobbyMember(FSteamId Lobby, FSteamId Member, FSteamId Owner) const;
	/** Notices a changed lobby or owner, for the party and the session lobby. Broadcasts On Lobby Owner Changed when the lobby stayed the same and the owner did not. */
	void RefreshLobbyOwners();
	void RefreshLobbyOwner(FTrackedLobby& Tracked, FSteamId Lobby, ESteamLobbyTarget Source);
	/** Reads the kick marker of the local player in Lobby. Leaves that lobby (or ends the session) and returns true when it is there. */
	bool CheckKicked(FSteamId Lobby);
	bool KickFromGameSession(FSteamId Member, const FText& Reason);

	/** Forgets the party lobby (after leaving it or being kicked): party settings and party match state. */
	void ClearParty();

	/** Reads OSTINGAME / OSTMATCH of the party and raises On Party Match Started / Ended when they changed. */
	void RefreshPartyMatch();
	void StartPartyMatchLocal(const FSteamPartyMatch& Match, const FString& Code, bool bFromCallback);
	void EndPartyMatchLocal();
	/** Joins or travels to Match (Session Lobby, Server Steam Id or Address). */
	FSteamResult FollowMatch(const FSteamPartyMatch& Match);
	/** Writes OSTINGAME = 0, removes OSTMATCH and restores the saved lobby state. Shared by End Party Match and the auto end. */
	FSteamResult EndPartyMatchInternal();
	/** The local player owns the party lobby (Steam.Error.NotOwner / Lobby.NotFound otherwise). */
	FSteamResult CheckPartyOwner() const;
	/** Host Party Session after its create finished: begins the match with the new session's lobby. Returns the final result. */
	FSteamResult FinishHostPartySession(const FSteamResult& Created);
	/** True (and counted) when the same lobby was requested within the duplicate window; otherwise records this request. */
	bool IsRepeatedRequest(const FSteamJoinRequest& Request);

	FSteamResult StartJoin(int32 ResultHandle, FSteamSessionJoinDelegate OnComplete);
	FSteamResult ClientTravelTo(const FString& Url);
	FSteamResult RequireClient() const;
	int32 GetProtectedHandle() const;

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	TSharedPtr<FSteamSessionsBackend> Backend;
	TSharedPtr<FSteamLobbyBackend> LobbyBackend;
	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	/** The party lobby (Create / Join Lobby). Invalid when there is none. */
	FSteamId PartyLobbyId;

	FTrackedLobby TrackedParty;
	FTrackedLobby TrackedSession;

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

	/** Profile of the session this game hosts (for the report and Get Current Steam Session Settings). Invalid without a profile. */
	FGameplayTag ActiveProfile;

	/** Settings this game applied while hosting the session. Only meaningful while hosting; reset on destroy / a failed create. */
	FSteamSessionSettings HostedSettings;

	/** Same two for the party lobby this game created. Reset when the party is left. */
	FGameplayTag PartyProfile;
	FSteamSessionSettings PartySettings;

	/** Joinable flag of the party as this game last set it (Steam cannot read it back). */
	bool bPartyJoinable = true;

	/** The party match as last seen in the party's lobby data (or from LobbyGameCreated_t before the data arrived). */
	bool bPartyMatchActive = false;
	bool bPartyMatchFromCallback = false;
	FString PartyMatchCode;
	FSteamPartyMatch PartyMatch;

	/** The request waiting for the game (Ask Game), and the one whose lobby is being looked up. */
	TOptional<FSteamJoinRequest> PendingRequest;
	TOptional<FSteamJoinRequest> LookupRequest;

	/** The request whose lobby data is awaited to tell a party from a session lobby. */
	TOptional<FSteamJoinRequest> PartyLookup;
	bool bPartyLookupSessionFallback = true;
	FTSTicker::FDelegateHandle PartyLookupTimeoutHandle;
	/** Time of the last request routed to a party, to drop the Online Subsystem's failed lookup of the same invite. */
	double LastPartyRequestTime = -1000.0;

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
