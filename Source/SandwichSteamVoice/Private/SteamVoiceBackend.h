// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Interfaces/VoiceInterface.h"

class FUniqueNetId;
class UGameInstance;

/**
 * The engine's Steam voice interface (IOnlineVoice: voice travels over the game NetConnection) plus one raw Steamworks question,
 * "is this player blocked on Steam". All methods run on the game thread; the Online Subsystem calls nothing back, so there are no delegates
 * and no callbacks here.
 */
class FSteamVoiceBackend
{
public:
	explicit FSteamVoiceBackend(UGameInstance* InGameInstance);

	FSteamVoiceBackend(const FSteamVoiceBackend&) = delete;
	FSteamVoiceBackend& operator=(const FSteamVoiceBackend&) = delete;

	/** The Steam Online Subsystem has a voice interface (needs [OnlineSubsystem] bHasVoiceEnabled=true, see Configure Steam). */
	bool IsUsable() const { return Voice.IsValid(); }

	/** Controller id of the first local player (0 when there is none yet). */
	int32 GetLocalUserNum() const;

	/** The signed in Steam user (raw). Invalid when Steam is not ready. */
	FSteamId GetLocalSteamId() const;

	/** Opens the microphone for the local player (StartNetworkedVoice). */
	bool StartTalking();

	/** Closes the microphone (StopNetworkedVoice). */
	bool StopTalking();

	bool IsHeadsetPresent();
	bool IsLocalTalking();
	bool IsRemoteTalking(const FUniqueNetId& Player);

	/** False when the backend did not accept the change (for example the talker is not registered yet). */
	bool SetMuted(const FUniqueNetId& Player, bool bMuted, bool bSystemWide);
	bool IsMuted(const FUniqueNetId& Player) const;

	/** True when the local user has blocked the player on Steam. False when unknown (Steam not ready). */
	bool IsBlockedOnSteam(FSteamId Player) const;

private:
	TWeakObjectPtr<UGameInstance> GameInstance;
	IOnlineVoicePtr Voice;
};
