// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "GameplayTagContainer.h"
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

/** Which voices a Steam Voice speaker plays. */
UENUM(BlueprintType)
enum class ESteamVoiceSpeakerSource : uint8
{
	/** The player who owns the pawn this speaker belongs to ("their mouth"; proximity voice with an attenuation). */
	Owner UMETA(DisplayName = "Owner"),
	/** Everyone transmitting on the speaker's channel, mixed (walkie, radio, PA). Works on any actor. */
	Channel UMETA(DisplayName = "Channel")
};

/**
 * A voice channel: a Gameplay Tag (mapped to an id in the Voice settings) or an id. The tag wins when it is set.
 * One struct everywhere, so one Blueprint pin takes either.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMVOICE_API FSteamVoiceChannel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (Categories = "Steam.Voice.Channel", ToolTip = "The channel as a Gameplay Tag. Wins over Id when set."))
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ClampMin = "-1", ToolTip = "The channel as a number, used when Tag is not set. -1 = no channel."))
	int32 Id = INDEX_NONE;

	/** True when a tag or a valid id is set (an unmapped tag still counts as set). */
	bool IsSet() const { return Tag.IsValid() || Id >= 0; }

	bool operator==(const FSteamVoiceChannel& Other) const { return Tag == Other.Tag && Id == Other.Id; }
};

/** A player (or the local player) started or stopped talking. Fires on the game thread. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamVoiceTalkingChanged, FSteamId, Player, bool, bTalking, bool, bIsLocal);

/** The voice mode changed (Set Voice Mode). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamVoiceModeChanged, ESteamVoiceMode, NewMode);

/** A player was muted or unmuted, by the game or automatically because the player is blocked on Steam. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamVoiceMuteChanged, FSteamId, Player, bool, bMuted, bool, bBecauseBlocked);
