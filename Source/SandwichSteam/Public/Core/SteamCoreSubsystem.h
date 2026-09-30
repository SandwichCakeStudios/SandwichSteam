// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "SteamCoreSubsystem.generated.h"

class FSteamCoreCallbacks;

/** Global state of the Steam host as seen by Sandwich Steam. */
UENUM(BlueprintType)
enum class ESteamState : uint8
{
	/** No Steam Online Subsystem, no Steamworks on this platform, or another GameInstance owns Steam. */
	Unavailable,
	/** Steam Online Subsystem exists but Steam is not logged on yet. */
	WaitingForSteam,
	/** Steam is up and logged on. Feature subsystems are active. */
	Ready,
	/** Connection to the Steam servers was lost. Features are inactive until it returns. */
	Offline,
	/** Steam is shutting down (client closed) or this subsystem is being destroyed. Terminal. */
	ShuttingDown
};

SANDWICHSTEAM_API const TCHAR* LexToString(ESteamState State);

/** What the game was asked to join on launch. */
UENUM(BlueprintType)
enum class ESteamJoinIntentType : uint8
{
	None,
	/** +connect_lobby <lobby id> */
	Lobby,
	/** +connect <address> */
	Server
};

/** Join request captured from the command line (Steam "Join Game" while the game was not running). */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamJoinIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Whether the launch asked to join a lobby, a server, or nothing."))
	ESteamJoinIntentType Type = ESteamJoinIntentType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Lobby to join when Type is Lobby."))
	FSteamId LobbyId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Server address to connect to when Type is Server."))
	FString ServerAddress;

	bool IsValid() const { return Type != ESteamJoinIntentType::None; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamStateChanged, ESteamState, NewState, ESteamState, OldState);

/**
 * Owns the global Steam state for one GameInstance: the ESteamState machine, the callback dispatcher and
 * the pending launch join intent. Feature subsystems depend on it and (de)activate on state changes.
 *
 * Steam is process-global, so only the first live GameInstance becomes the Steam owner. Other GameInstances
 * (multi-PIE clients) stay Unavailable.
 *
 * Steam itself is initialized by OnlineSubsystemSteam. This subsystem only observes it.
 */
UCLASS()
class SANDWICHSTEAM_API USteamCoreSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Core subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamCoreSubsystem* Get(const UObject* WorldContext);

	//~ Begin USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** Current Steam state. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (ToolTip = "Current state of Steam: Unavailable, WaitingForSteam, Ready, Offline or ShuttingDown."))
	ESteamState GetSteamState() const { return SteamState; }

	/** True when the state is Ready. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (ToolTip = "True when Steam is up and logged on."))
	bool IsSteamReady() const { return SteamState == ESteamState::Ready; }

	/** True when this GameInstance is the one that owns Steam in this process. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (ToolTip = "True when this game instance owns Steam. Steam is process-global, so only the first game instance (for example the first PIE client) does."))
	bool IsSteamOwner() const { return bIsSteamOwner; }

	/** Broadcast on every state change (game thread). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Core", meta = (ToolTip = "Called when the Steam state changes."))
	FOnSteamStateChanged OnSteamStateChanged;

	/** True when the launch command line asked to join a lobby or server and it was not consumed yet. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (ToolTip = "True when the game was launched to join a lobby or server and that request has not been consumed yet."))
	bool HasPendingJoinIntent() const { return PendingJoinIntent.IsValid(); }

	/** Returns the pending join request and clears it. False when there is none. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Core", meta = (ToolTip = "Returns the join request from the launch command line (+connect_lobby / +connect) and clears it. Returns false when there is none."))
	bool ConsumePendingJoinIntent(FSteamJoinIntent& OutIntent);

	/** Dispatcher that moves raw Steam callbacks onto the game thread. Null before Initialize(). */
	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> GetDispatcher() const { return Dispatcher; }

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line state report for Steam.Core.Dump. */
	FString BuildDebugString() const;

	/** Starts a raw Steam call round trip and logs on which thread its callback ran. */
	void RunThreadCheck();
#endif

private:
	friend class FSteamCoreCallbacks;

	bool TryClaimOwnership();
	void ReleaseOwnership();
	void EvaluateReadiness();
	void SetSteamState(ESteamState NewState);
	void ParseLaunchJoinIntent();
	void ScheduleServerReadinessPoll();

	/** Game-thread handlers for the raw Steam callbacks (invoked through the dispatcher). */
	void HandleSteamServersConnected();
	void HandleSteamServersDisconnected(int32 NativeResult);
	void HandleSteamShutdown();

	ESteamState SteamState = ESteamState::Unavailable;
	bool bIsSteamOwner = false;
	bool bJoinIntentParsed = false;
	FSteamJoinIntent PendingJoinIntent;

	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
	TSharedPtr<FSteamCoreCallbacks> Callbacks;

	FTSTicker::FDelegateHandle ServerPollHandle;
	int32 ServerPollAttempts = 0;
};
