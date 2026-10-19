// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamVoiceTypes.h"
#include "SteamVoiceLibrary.generated.h"

class APlayerController;
class USteamVoiceComponent;

/**
 * In-session voice chat for Blueprints. The events (On Player Talking Changed, On Voice Mode Changed, On Player Mute Changed) are on the
 * Steam Voice Subsystem: Get Game Instance Subsystem > Steam Voice Subsystem.
 * Voice needs two players in the same game session (listen server, dedicated server, or a lobby that travelled to a map).
 * Every function returns a result: Steam.Error.FeatureDisabled when the Voice feature is not available.
 */
UCLASS()
class SANDWICHSTEAMVOICE_API USteamVoiceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Turns voice chat on or off. Off closes the microphone and stops all voice work."))
	static FSteamResult SetSteamVoiceEnabled(const UObject* WorldContextObject, bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True while voice chat is on."))
	static bool IsSteamVoiceEnabled(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Switches between push to talk and open mic. Takes effect at once."))
	static FSteamResult SetSteamVoiceMode(const UObject* WorldContextObject, ESteamVoiceMode Mode);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "The current voice mode."))
	static ESteamVoiceMode GetSteamVoiceMode(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the microphone (push to talk). Only needed when you do not set a Push To Talk Action in the Voice settings."))
	static FSteamResult StartSteamTalking(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Closes the microphone (push to talk). In open mic mode the microphone stays open."))
	static FSteamResult StopSteamTalking(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Binds the Push To Talk Action of the Voice settings on the controller's Enhanced Input component. Automatic when Auto Bind Push To Talk is on; call it yourself when it is off."))
	static FSteamResult BindSteamPushToTalk(const UObject* WorldContextObject, APlayerController* Controller);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the push to talk action is bound to the local controller."))
	static bool IsSteamPushToTalkBound(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the local microphone picks up voice. Use it for a 'you are talking' indicator."))
	static bool IsSteamLocalPlayerTalking(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True when a headset or microphone is present, as far as Steam tells. Not reliable with the Steam voice pipeline: Steam reports a missing microphone like silence. Show Get Steam Local Mic Level while the player talks instead."))
	static bool IsSteamHeadsetPresent(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Microphone sensitivity: audio quieter than this level (0 to 1, 0 = off) is not sent. Use it with Get Steam Local Mic Level for an options slider. Steam voice pipeline only."))
	static FSteamResult SetSteamVoiceActivationThreshold(const UObject* WorldContextObject, float Threshold);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "The current voice activation threshold (0 = off)."))
	static float GetSteamVoiceActivationThreshold(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Level of the local microphone (0 to 1, same scale as the activation threshold) while it is open and picks up sound, else 0. Drive a mic meter with it. Steam voice pipeline only."))
	static float GetSteamLocalMicLevel(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "The channels you talk on (walkie, radio, PA), at most 8. Everyone's Channel speakers on these channels play your voice. Empty = no channel. Steam voice pipeline only."))
	static FSteamResult SetSteamVoiceTransmitChannels(const UObject* WorldContextObject, const TArray<FSteamVoiceChannel>& Channels);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "Starts talking on one more channel (press the radio key). Nothing happens when you already talk on it."))
	static FSteamResult AddSteamVoiceTransmitChannel(const UObject* WorldContextObject, FSteamVoiceChannel Channel);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "Stops talking on a channel (release the radio key)."))
	static FSteamResult RemoveSteamVoiceTransmitChannel(const UObject* WorldContextObject, FSteamVoiceChannel Channel);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "Stops talking on every channel. Your voice still plays on your own pawn while Transmit To Owner is on."))
	static FSteamResult ClearSteamVoiceTransmitChannels(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "The channels you talk on."))
	static TArray<FSteamVoiceChannel> GetSteamVoiceTransmitChannels(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "True when you talk on the channel."))
	static bool IsSteamVoiceTransmittingOn(const UObject* WorldContextObject, FSteamVoiceChannel Channel);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Channels", meta = (GameplayTagFilter = "Steam.Voice.Channel", ToolTip = "A voice channel from a Steam.Voice.Channel tag. The tag needs an id in Channel Tag Map (Project Settings > Plugins > Sandwich Steam - Voice)."))
	static FSteamVoiceChannel MakeSteamVoiceChannelFromTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Channels", meta = (ToolTip = "A voice channel from a number (0 or more), for channels you pick at runtime (a walkie dial, a team index)."))
	static FSteamVoiceChannel MakeSteamVoiceChannelFromId(int32 Id);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Channels", meta = (ToolTip = "The id the channel travels as: the tag's id from Channel Tag Map, or the channel's own id. -1 when the channel is not set or its tag is not mapped."))
	static int32 ResolveSteamVoiceChannelId(FSteamVoiceChannel Channel);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "Whether your voice plays on your own pawn (its Owner speakers, proximity). On by default; off with a channel = radio only. Steam voice pipeline only."))
	static FSteamResult SetSteamVoiceTransmitToOwner(const UObject* WorldContextObject, bool bTransmit);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "True while your voice plays on your own pawn."))
	static bool IsSteamVoiceTransmittingToOwner(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the player talks (your own Steam ID too). Muted players only with Report Muted Talking in the Voice settings."))
	static bool IsSteamPlayerTalking(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "The channels the player talks on right now; empty while they do not talk. A channel id with tags in Channel Tag Map comes back as those tags. Your own Steam ID gives what you are sent on. Steam voice pipeline only."))
	static TArray<FSteamVoiceChannel> GetSteamPlayerTransmitChannels(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Transmit", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the player talks on the channel (show 'Blue is on the radio'). Tags compare by their id. Steam voice pipeline only."))
	static bool IsSteamPlayerTransmittingOn(const UObject* WorldContextObject, FSteamId Player, FSteamVoiceChannel Channel);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Everybody who talks right now, the local player included."))
	static TArray<FSteamId> GetSteamTalkingPlayers(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "The Steam Voice speakers that play the player right now: their pawn's Owner speakers, or the 2D speaker while they have no pawn. Empty until their voice arrived. Steam voice pipeline only."))
	static TArray<USteamVoiceComponent*> GetSteamPlayerVoiceComponents(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Mutes a player's voice for this game. The mute is kept until the game closes and applies as soon as the player is in the game. System Wide asks for a mute that also affects other games (Steam treats it like a game mute)."))
	static FSteamResult MuteSteamPlayer(const UObject* WorldContextObject, FSteamId Player, bool bSystemWide = false);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Removes a mute you set. A player you blocked on Steam stays muted while Auto Mute Blocked Players is on."))
	static FSteamResult UnmuteSteamPlayer(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the player is muted, by you or automatically because you blocked them on Steam."))
	static bool IsSteamPlayerMuted(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Everybody who is muted."))
	static TArray<FSteamId> GetSteamMutedPlayers(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Volume of all voice audio, 0 to 2 (1 = unchanged). Needs a Voice Sound Class and Voice Sound Mix in the Voice settings."))
	static FSteamResult SetSteamVoiceMasterVolume(const UObject* WorldContextObject, float Volume);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "The master volume of the voice audio."))
	static float GetSteamVoiceMasterVolume(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Volume of one player's voice for you, 0 to 2 (1 = unchanged). Covers their pawn and every channel they talk on. 0 silences them but keeps their talking indicator (unlike a mute). Kept until the game closes. Steam voice pipeline only."))
	static FSteamResult SetSteamPlayerVoiceVolume(const UObject* WorldContextObject, FSteamId Player, float Volume = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "The volume you set for the player's voice, 1 when none was set."))
	static float GetSteamPlayerVoiceVolume(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Loudness of the player's voice as it plays (0 to 1, same scale as Get Steam Local Mic Level), before the volume you set for them; 0 while they do not talk or are muted. Your own Steam ID gives your microphone level. Drive a per player meter with it. Steam voice pipeline only."))
	static float GetSteamPlayerVoiceLevel(const UObject* WorldContextObject, FSteamId Player);
};
