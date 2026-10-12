// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamId.h"
#include "SteamAuthSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class FOnlineAuthUtilsSteam;
class FUniqueNetId;
enum class ESteamAuthResponseCode : uint8;

/**
 * Steam authentication of joining players (Phase 16, probe step).
 *
 * The engine does the check: the SteamAuth packet handler component
 * ([PacketHandlerComponents] +Components=OnlineSubsystemSteam.SteamAuthComponentModuleInterface) runs Steam's ticket
 * validation for every incoming connection and kicks players who fail. This subsystem only listens: it owns
 * FOnlineAuthUtilsSteam::OnAuthenticationResultWithCodeDelegate (single-cast, so never bind it elsewhere) and records
 * every result on the host. The engine's failure behaviour (kick) is not changed.
 *
 * Inactive when the default online subsystem is not Steam or the packet handler line is missing.
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
		bool bSuccess = false;
		uint8 ResponseCode = 0;
		double ResultSeconds = 0.0;
		double PostLoginSeconds = 0.0;
		bool bResultOnGameThread = true;
	};

	/** OnAuthenticationResultWithCodeDelegate target. Thread not guaranteed by the OSS, so it is checked and logged. */
	void HandleAuthResult(const FUniqueNetId& UserId, bool bWasSuccess, ESteamAuthResponseCode ResponseCode);

	/** Game thread. */
	void RecordAuthResult(FSteamId Player, bool bWasSuccess, uint8 ResponseCode, bool bOnGameThread);

	/** FGameModeEvents::GameModePostLoginEvent target: logs whether the auth result came before or after PostLogin. */
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	TWeakPtr<FOnlineAuthUtilsSteam, ESPMode::ThreadSafe> AuthUtils;
	TMap<FSteamId, FAuthRecord> Records;
	FDelegateHandle PostLoginHandle;

	bool bPacketHandlerEnabled = false;
	bool bBoundResultDelegate = false;
};
