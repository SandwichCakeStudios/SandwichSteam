// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"
#include "SteamSessionsAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamSessionsFoundAsyncDelegate, const TArray<FSteamSessionResult>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamSessionJoinedAsyncDelegate, const FString&, ConnectString);

/** Hosts a session of a profile of the Steam App Definition. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamCreateSessionAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Session", DisplayName = "Create Steam Session", ToolTip = "Hosts a session (a Steam lobby) described by a session profile of the Steam App Definition. Afterwards load the map with Steam Server Travel."))
	static USteamCreateSessionAsyncAction* CreateSteamSession(const UObject* WorldContextObject, FGameplayTag ProfileTag);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;

private:
	FGameplayTag ProfileTag;
};

/** Searches Steam lobbies. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamFindSessionsAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the sessions that were found, possibly none. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called with the sessions that were found (possibly none)."))
	FSteamSessionsFoundAsyncDelegate OnSessionsFound;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Options", DisplayName = "Find Steam Sessions", ToolTip = "Searches Steam lobbies. Results stay valid until the next search."))
	static USteamFindSessionsAsyncAction* FindSteamSessions(const UObject* WorldContextObject, const FSteamSessionSearchOptions& Options);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamSessionSearchOptions Options;
	TArray<FSteamSessionResult> Found;
};

/** Joins a session found by Find Steam Sessions. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamJoinSessionAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the address of the host. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called with the connect string of the host. The player travels by itself when Auto Travel After Join is on."))
	FSteamSessionJoinedAsyncDelegate OnJoined;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Join Steam Session", ToolTip = "Joins a session from Find Steam Sessions. Fails with Lobby Full or Lobby Not Found when the session changed since the search."))
	static USteamJoinSessionAsyncAction* JoinSteamSession(const UObject* WorldContextObject, const FSteamSessionResult& Session);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamSessionResult Session;
	FString ConnectString;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamSessionSettingsAppliedAsyncDelegate, const FSteamSessionSettings&, AppliedSettings);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamLobbySettingsAppliedAsyncDelegate, FSteamId, LobbyId, const FSteamSessionSettings&, AppliedSettings);

/** Hosts a session from a plain settings struct: full player control, no App Definition row and no rules. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamCreateSessionWithSettingsAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the settings Steam actually applied. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|By Settings", meta = (ToolTip = "Called with the settings Steam actually applied."))
	FSteamSessionSettingsAppliedAsyncDelegate OnCreated;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Settings", DisplayName = "Create Steam Session With Settings", ToolTip = "Hosts a session from a plain settings struct: full player control, no session profile and no rules. Afterwards load the map with Steam Server Travel."))
	static USteamCreateSessionWithSettingsAsyncAction* CreateSteamSessionWithSettings(const UObject* WorldContextObject, const FSteamSessionSettings& Settings);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamSessionSettings Settings;
	FSteamSessionSettings Applied;
};

/** Hosts a session of a profile with player-chosen settings; the profile's optional rules clamp or refuse them. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamCreateSessionFromProfileAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the settings Steam actually applied. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|By Settings", meta = (ToolTip = "Called with the settings Steam actually applied."))
	FSteamSessionSettingsAppliedAsyncDelegate OnCreated;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Session", AutoCreateRefTerm = "Settings", DisplayName = "Create Steam Session From Profile", ToolTip = "Hosts a session of a profile of the Steam App Definition with player-chosen settings. Values outside the profile's optional guardrails are clamped or refused; tooltips on Steam Session Settings explain each field. Afterwards load the map with Steam Server Travel."))
	static USteamCreateSessionFromProfileAsyncAction* CreateSteamSessionFromProfile(const UObject* WorldContextObject, FGameplayTag ProfileTag, const FSteamSessionSettings& Settings);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FGameplayTag ProfileTag;
	FSteamSessionSettings Settings;
	FSteamSessionSettings Applied;
};

/** Changes the settings of the running session (or lobby only lobby). Host / lobby owner only. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamUpdateSessionAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the settings Steam actually applied. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|By Settings", meta = (ToolTip = "Called with the settings Steam actually applied."))
	FSteamSessionSettingsAppliedAsyncDelegate OnUpdated;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Settings", DisplayName = "Update Steam Session", ToolTip = "Changes the settings of the running session or lobby only lobby (name, visibility, slots, extra settings). Host / lobby owner only (Steam.Error.NotOwner otherwise). Rules are guardrails checked on the host: a modified client can skip them."))
	static USteamUpdateSessionAsyncAction* UpdateSteamSession(const UObject* WorldContextObject, const FSteamSessionSettings& Settings);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamSessionSettings Settings;
	FSteamSessionSettings Applied;
};

/** Leaves the current session (closes it when hosting). */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamDestroySessionAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Destroy Steam Session", ToolTip = "Leaves the current session, or closes it when this game hosts it. Succeeds at once when there is no session."))
	static USteamDestroySessionAsyncAction* DestroySteamSession(const UObject* WorldContextObject);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamLobbyIdAsyncDelegate, FSteamId, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamLobbiesFoundAsyncDelegate, const TArray<FSteamLobbyInfo>&, Lobbies);

/** Creates a lobby without a net session (lobby only mode). */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamCreateLobbyAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the Steam ID of the new lobby. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called with the Steam ID of the new lobby."))
	FSteamLobbyIdAsyncDelegate OnLobbyEntered;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Session", DisplayName = "Create Steam Lobby", ToolTip = "Creates a Steam lobby without a net session (chat, lobby data and a ready check, for example a character select before the match). Uses the slots, visibility and settings of a session profile. Exclusive with a session."))
	static USteamCreateLobbyAsyncAction* CreateSteamLobby(const UObject* WorldContextObject, FGameplayTag ProfileTag);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FGameplayTag ProfileTag;
	FSteamId LobbyId;
};

/** Creates a lobby without a net session, from a plain settings struct: full player control, no profile and no rules. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamCreateLobbyWithSettingsAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the Steam ID of the new lobby and the settings Steam actually applied. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|By Settings", meta = (ToolTip = "Called with the Steam ID of the new lobby and the settings Steam actually applied."))
	FSteamLobbySettingsAppliedAsyncDelegate OnLobbyEntered;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Settings", DisplayName = "Create Steam Lobby With Settings", ToolTip = "Creates a Steam lobby without a net session, from a plain settings struct: full player control, no session profile and no rules. Exclusive with a session."))
	static USteamCreateLobbyWithSettingsAsyncAction* CreateSteamLobbyWithSettings(const UObject* WorldContextObject, const FSteamSessionSettings& Settings);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamSessionSettings Settings;
	FSteamId LobbyId;
	FSteamSessionSettings Applied;
};

/** Creates a lobby without a net session, from a profile with player-chosen settings. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamCreateLobbyFromProfileAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the Steam ID of the new lobby and the settings Steam actually applied. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions|By Settings", meta = (ToolTip = "Called with the Steam ID of the new lobby and the settings Steam actually applied."))
	FSteamLobbySettingsAppliedAsyncDelegate OnLobbyEntered;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions|By Settings", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", Categories = "Steam.Session", AutoCreateRefTerm = "Settings", DisplayName = "Create Steam Lobby From Profile", ToolTip = "Creates a Steam lobby without a net session, from a session profile with player-chosen settings. Values outside the profile's optional guardrails are clamped or refused. Exclusive with a session."))
	static USteamCreateLobbyFromProfileAsyncAction* CreateSteamLobbyFromProfile(const UObject* WorldContextObject, FGameplayTag ProfileTag, const FSteamSessionSettings& Settings);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FGameplayTag ProfileTag;
	FSteamSessionSettings Settings;
	FSteamId LobbyId;
	FSteamSessionSettings Applied;
};

/** Joins a lobby by its Steam ID (lobby only mode). */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamJoinLobbyAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the Steam ID of the lobby that was entered. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called with the Steam ID of the lobby that was entered."))
	FSteamLobbyIdAsyncDelegate OnLobbyEntered;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Join Steam Lobby", ToolTip = "Joins a Steam lobby by its Steam ID (lobby only mode, no net session). Fails with Lobby Full, Lobby Not Found or Lobby Denied."))
	static USteamJoinLobbyAsyncAction* JoinSteamLobby(const UObject* WorldContextObject, FSteamId LobbyId);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamId LobbyId;
	FSteamId EnteredId;
};

/** Searches lobby only lobbies. */
UCLASS()
class SANDWICHSTEAMSESSIONS_API USteamFindLobbiesAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the lobbies that were found, possibly none. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Sessions", meta = (ToolTip = "Called with the lobbies that were found (possibly none)."))
	FSteamLobbiesFoundAsyncDelegate OnLobbiesFound;

	UFUNCTION(BlueprintCallable, Category = "Steam|Sessions", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "Options", DisplayName = "Find Steam Lobbies", ToolTip = "Searches Steam lobbies (lobby only mode). Only lobbies with a free slot. Lobbies of net sessions are found with Find Steam Sessions."))
	static USteamFindLobbiesAsyncAction* FindSteamLobbies(const UObject* WorldContextObject, const FSteamSessionSearchOptions& Options);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FSteamSessionSearchOptions Options;
	TArray<FSteamLobbyInfo> Found;
};
