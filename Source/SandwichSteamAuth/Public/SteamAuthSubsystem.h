// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "SteamAuthTypes.h"
#include "SteamAuthSubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class APlayerState;
class FOnlineAuthUtilsSteam;
class FUniqueNetId;
class UWorld;
enum class ESteamAuthResponseCode : uint8;

/**
 * Steam authentication of joining players.
 *
 * The engine does the check: the SteamAuth packet handler component
 * ([PacketHandlerComponents] +Components=OnlineSubsystemSteam.SteamAuthComponentModuleInterface, written by Configure Steam when
 * Enable Steam Authentication is on) validates the Steam ticket of every incoming connection. This subsystem reports the results
 * on the host and, with the Custom failure policy, takes over what happens to players who fail.
 *
 * It owns FOnlineAuthUtilsSteam::OnAuthenticationResultWithCodeDelegate and (Custom only) OverrideFailureDelegate. Both are
 * single-cast: use On Steam Auth Result / On Steam Auth Failed and never bind them elsewhere. When other code bound one first,
 * the subsystem logs a warning and leaves it alone.
 *
 * Inactive (no cost) when the default online subsystem is not Steam or the packet handler line is missing. The runtime trusts what
 * the engine loaded, not the setting. Results arrive on the host only, after PostLogin (verified 2026-10-13), on the game thread.
 */
UCLASS()
class SANDWICHSTEAMAUTH_API USteamAuthSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Auth subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamAuthSubsystem* Get(const UObject* WorldContext);

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	//~ End USteamFeatureSubsystem

	/** True when the engine loaded the SteamAuth packet handler component (what FOnlineAuthUtilsSteam reports). */
	bool IsPacketHandlerEnabled() const { return bPacketHandlerEnabled; }

	/** The failure policy read when the feature started (Project Settings > Sandwich Steam - Auth). */
	ESteamAuthFailurePolicy GetFailurePolicy() const { return ActivePolicy; }

	/** Where the player is in the Steam check on this host. NotChecked when the feature is inactive or the player is unknown. */
	ESteamAuthStatus GetAuthStatus(FSteamId Player) const;

	/** The last result Steam sent for the player. False when there is none. */
	bool GetLastResult(FSteamId Player, FSteamAuthResult& OutResult) const;

	/**
	 * True when Steam accepted the player. Players on this machine (the host and its split screen players) are never checked and
	 * count as authenticated. Host only: a client has no results.
	 */
	bool IsPlayerAuthenticated(const APlayerState* PlayerState) const;

	/** Kicks a connected player through AGameSession::KickPlayer. Host only. Meant for the Custom policy. */
	FSteamResult KickPlayer(FSteamId Player, const FText& Reason);

	/** Steam answered for a joining player (passed or failed). Host only, game thread. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Auth", meta = (ToolTip = "Steam answered for a joining player: passed or failed, with the reason. Host only. Fires shortly after the player's PostLogin."))
	FSteamAuthResultEvent OnSteamAuthResult;

	/** A player failed and the Custom policy is on: nobody was kicked. Also fires for a player without authentication. Host only. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Auth", meta = (ToolTip = "Custom failure policy only: a player failed Steam authentication (or joined without it) and was NOT kicked. Decide here, for example with Kick Steam Player. Host only."))
	FSteamAuthFailedEvent OnSteamAuthFailed;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Auth.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	struct FAuthRecord
	{
		ESteamAuthStatus Status = ESteamAuthStatus::NotChecked;
		/** Valid once Steam sent a result (a failure without a result has none). */
		bool bHasResult = false;
		ESteamAuthResponse Response = ESteamAuthResponse::Unknown;
		double ResultSeconds = 0.0;
		double PostLoginSeconds = 0.0;
		bool bResultOnGameThread = true;
	};

	/** OnAuthenticationResultWithCodeDelegate target. Marshals to the game thread when needed. */
	void HandleAuthResult(const FUniqueNetId& UserId, bool bWasSuccess, ESteamAuthResponseCode ResponseCode);

	/** OverrideFailureDelegate target (Custom policy). Marshals to the game thread when needed. */
	void HandleAuthFailure(const FUniqueNetId& UserId);

	/** Game thread. */
	void RecordAuthResult(FSteamId Player, bool bWasSuccess, ESteamAuthResponse Response, bool bOnGameThread);
	void RecordAuthFailure(FSteamId Player);

	/** FGameModeEvents targets: Pending at PostLogin, forget on Logout. */
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);

	/** Forgets every player when this game instance's world ends a session (map load, return to menu). */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	bool IsOwnGameMode(const AGameModeBase* GameMode) const;

	TWeakPtr<FOnlineAuthUtilsSteam, ESPMode::ThreadSafe> AuthUtils;
	TMap<FSteamId, FAuthRecord> Records;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle WorldCleanupHandle;

	ESteamAuthFailurePolicy ActivePolicy = ESteamAuthFailurePolicy::Kick;
	bool bPacketHandlerEnabled = false;
	bool bBoundResultDelegate = false;
	bool bBoundFailureDelegate = false;
	bool bWarnedOffGameThread = false;
};
