// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceInterface.h"

class FUniqueNetId;
class UGameInstance;

/**
 * Fallback voice path: the engine's Steam voice interface (IOnlineVoice, voice travels over the game NetConnection).
 * Used instead of the Steam voice pipeline when the project enables the Online Subsystem voice ([OnlineSubsystem] bHasVoiceEnabled=true);
 * both would read Steam's microphone, so only one runs. All methods run on the game thread; the Online Subsystem calls nothing back,
 * so there are no delegates and no callbacks here.
 */
class FSteamVoiceOssFallback
{
public:
	explicit FSteamVoiceOssFallback(UGameInstance* InGameInstance);

	FSteamVoiceOssFallback(const FSteamVoiceOssFallback&) = delete;
	FSteamVoiceOssFallback& operator=(const FSteamVoiceOssFallback&) = delete;

	/** The Steam Online Subsystem has a voice interface (needs [OnlineSubsystem] bHasVoiceEnabled=true). */
	bool IsUsable() const { return Voice.IsValid(); }

	/** Controller id of the first local player (0 when there is none yet). */
	int32 GetLocalUserNum() const;

	/** Opens the microphone for the local player (StartNetworkedVoice). */
	bool StartTalking();

	/** Closes the microphone (StopNetworkedVoice). */
	bool StopTalking();

	bool IsHeadsetPresent();
	bool IsLocalTalking();
	bool IsRemoteTalking(const FUniqueNetId& Player);

	/** False when the Online Subsystem did not accept the change (for example the talker is not registered yet). */
	bool SetMuted(const FUniqueNetId& Player, bool bMuted, bool bSystemWide);
	bool IsMuted(const FUniqueNetId& Player) const;

private:
	TWeakObjectPtr<UGameInstance> GameInstance;
	IOnlineVoicePtr Voice;
};
