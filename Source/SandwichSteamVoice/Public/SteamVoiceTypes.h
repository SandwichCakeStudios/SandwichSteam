// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "GameplayTagContainer.h"
#include "SteamVoiceTypes.generated.h"

class APlayerState;

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

/** How a player is heard while they have no pawn (menus, lobby, spectating, between death and respawn). */
UENUM(BlueprintType)
enum class ESteamVoicePawnlessMode : uint8
{
	/** One unattached 2D speaker plays them: heard everywhere at full volume. */
	Play2D UMETA(DisplayName = "2D"),
	/** Not heard until they have a pawn again. Their talking events still fire. */
	Silent UMETA(DisplayName = "Silent")
};

/** How voice travels between the players (Server Authority). */
UENUM(BlueprintType)
enum class ESteamVoiceTopology : uint8
{
	/** Every player sends to every other player directly. Lowest latency; the rules hold against single cheaters only. */
	PeerToPeer UMETA(DisplayName = "Peer To Peer"),
	/** Players send to the host only; the host checks the rules and forwards. Private against cheaters working together; costs one extra hop and the host's upload. */
	ServerRelay UMETA(DisplayName = "Server Relay")
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

	/** The id this channel plays on: the id TagMap gives Tag when Tag is set, else Id. INDEX_NONE when unset, unmapped or negative. */
	int32 Resolve(const TMap<FGameplayTag, int32>& TagMap) const
	{
		if (Tag.IsValid())
		{
			const int32* Mapped = TagMap.Find(Tag);
			return Mapped && *Mapped >= 0 ? *Mapped : INDEX_NONE;
		}
		return Id >= 0 ? Id : INDEX_NONE;
	}

	bool operator==(const FSteamVoiceChannel& Other) const { return Tag == Other.Tag && Id == Other.Id; }
};

/** Which channels a server rule allows a player to talk or listen on (Server Authority). */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMVOICE_API FSteamVoiceChannelRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ToolTip = "Off: every channel. On: only the channels below; on with an empty list: no channel at all."))
	bool bRestricted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (EditCondition = "bRestricted", ToolTip = "The allowed channels (only used when Restricted). Every client resolves tags with its own Channel Tag Map, so all builds need the same map."))
	TArray<FSteamVoiceChannel> Channels;

	bool IsDefault() const { return !bRestricted; }

	bool operator==(const FSteamVoiceChannelRule& Other) const { return bRestricted == Other.bRestricted && Channels == Other.Channels; }
	bool operator!=(const FSteamVoiceChannelRule& Other) const { return !(*this == Other); }
};

/**
 * What the server allows one player (Server Authority in the Voice settings). Every client follows it on both ends: the player's own client
 * does not send what is forbidden, and everyone else drops it.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMVOICE_API FSteamVoicePlayerRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ToolTip = "The player may talk. Off = server mute: their microphone closes and everyone drops their voice."))
	bool bCanTalk = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ToolTip = "The player's voice may play on their own pawn (Owner speakers, proximity). Off = only their channels play (dead players, spectators)."))
	bool bCanUseOwnerVoice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ToolTip = "The channels the player may talk on (team radio, dead chat). Channels they transmit on outside it are not sent and are dropped by everyone."))
	FSteamVoiceChannelRule Transmit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ToolTip = "The channels the player may hear. Nobody sends them other channels, and their Channel speakers on other channels stay silent."))
	FSteamVoiceChannelRule Listen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam Voice", meta = (ClampMin = "-1", Units = "cm", ToolTip = "How far this player's own voice (their pawn) carries. 0 = the game's Owner Voice Max Distance; below 0 = no limit (megaphone)."))
	float OwnerVoiceMaxDistance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Steam Voice", meta = (ToolTip = "Set by the server: another player in the game has the same Steam ID. This Steam ID is neither sent nor played until one of them leaves (On Voice Identity Conflict on the server)."))
	bool bIdentityConflict = false;

	/** True when nothing is restricted (the rules every player has until the server says otherwise). */
	bool IsDefault() const { return bCanTalk && bCanUseOwnerVoice && Transmit.IsDefault() && Listen.IsDefault() && OwnerVoiceMaxDistance == 0.0f && !bIdentityConflict; }

	/** May talk at all: not server muted and no identity conflict. */
	bool MayTalk() const { return bCanTalk && !bIdentityConflict; }

	bool operator==(const FSteamVoicePlayerRules& Other) const
	{
		return bCanTalk == Other.bCanTalk && bCanUseOwnerVoice == Other.bCanUseOwnerVoice && Transmit == Other.Transmit && Listen == Other.Listen
			&& OwnerVoiceMaxDistance == Other.OwnerVoiceMaxDistance && bIdentityConflict == Other.bIdentityConflict;
	}
	bool operator!=(const FSteamVoicePlayerRules& Other) const { return !(*this == Other); }
};

/** A player (or the local player) started or stopped talking. Fires on the game thread. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamVoiceTalkingChanged, FSteamId, Player, bool, bTalking, bool, bIsLocal);

/** The voice mode changed (Set Voice Mode). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamVoiceModeChanged, ESteamVoiceMode, NewMode);

/** The game allowed or forbade voice (Set Voice Allowed). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamVoiceAllowedChanged, bool, bAllowed);

/** A player was muted or unmuted, by the game or automatically because the player is blocked on Steam. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamVoiceMuteChanged, FSteamId, Player, bool, bMuted, bool, bBecauseBlocked);

/** The channels a player's voice carries changed (they started or stopped talking on a radio, or switched channels). Empty once they stopped talking. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamVoiceTransmitChannelsChanged, FSteamId, Player, const TArray<FSteamVoiceChannel>&, Channels);

/** A player sent too much voice data for 5 seconds in a row and is muted for the session. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamVoicePlayerFlood, FSteamId, Player);

/** The server's voice rules arrived or changed (Server Authority). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamVoiceRulesChanged);

/**
 * Server, Server Relay: where a forwarded player's voice goes changed (after the rules and the relay filter). bTalking false once they stopped
 * (Owner Voice false, Channels empty). Fires on changes only, never per packet.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSteamVoiceRelayChanged, FSteamId, Player, bool, bTalking, bool, bOwnerVoice, const TArray<FSteamVoiceChannel>&, Channels);

/** Server: a player joined with a Steam ID another player already has (Server Authority). Joining is the newer player state. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamVoiceIdentityConflict, FSteamId, Player, APlayerState*, Joining);
