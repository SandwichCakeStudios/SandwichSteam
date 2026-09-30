// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "SteamVoiceTypes.generated.h"

/** How the local microphone is opened. */
UENUM(BlueprintType)
enum class ESteamVoiceMode : uint8
{
	/** The microphone is open only while the push to talk input is held (or Start Talking / Stop Talking was called). */
	PushToTalk UMETA(DisplayName = "Push To Talk"),
	/** The microphone is always open while voice is enabled. */
	OpenMic UMETA(DisplayName = "Open Mic")
};

/** A player (or the local player) started or stopped talking. Fires on the game thread. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamVoiceTalkingChanged, FSteamId, Player, bool, bTalking, bool, bIsLocal);

/** The voice mode changed (Set Voice Mode). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamVoiceModeChanged, ESteamVoiceMode, NewMode);

/** A player was muted or unmuted, by the game or automatically because the player is blocked on Steam. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamVoiceMuteChanged, FSteamId, Player, bool, bMuted, bool, bBecauseBlocked);
