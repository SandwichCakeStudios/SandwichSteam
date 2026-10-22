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

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Game Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "The game's voice rule, separate from the player's Set Steam Voice Enabled option: forbid voice in a lobby, allow it in the match. Not allowed: nothing is sent or played; the player's setting and the mic test stay. Without Server Authority it is local to this client: call it on every client (for example from replicated game state). With Server Authority call it on the server only; it reaches every client. Online Subsystem voice: only closes the microphone."))
	static FSteamResult SetSteamVoiceAllowed(const UObject* WorldContextObject, bool bAllowed);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Game Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the game allows voice."))
	static bool IsSteamVoiceAllowed(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Game Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "True while lobby voice runs (Allow Voice In Lobby in the Voice settings): the local player is in a Steam lobby and there is no networked game yet, so the lobby members are heard (2D, peer to peer). Show a 'lobby voice' icon with it. Steam voice pipeline only."))
	static bool IsSteamLobbyVoiceActive(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Game Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "How players are heard while they have no pawn (menus, lobby, spectating, between death and respawn). 2D: heard everywhere. Silent: not heard until they have a pawn again (proximity games). For example 2D in the lobby, Silent in the match. Without Server Authority local to this client; with it call it on the server only. Steam voice pipeline only."))
	static FSteamResult SetSteamPawnlessVoice(const UObject* WorldContextObject, ESteamVoicePawnlessMode Mode);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Game Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "How players without a pawn are heard right now."))
	static ESteamVoicePawnlessMode GetSteamPawnlessVoice(const UObject* WorldContextObject);

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

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Mic test for an options menu: opens the microphone and plays your voice back to you (2D, one playout delay late, after the activation threshold), so you hear what others would hear. Sends nothing to other players by itself. Needs voice enabled. Steam voice pipeline only."))
	static FSteamResult StartSteamVoiceMicTest(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Ends the mic test (closing your options menu)."))
	static FSteamResult StopSteamVoiceMicTest(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the mic test runs."))
	static bool IsSteamVoiceMicTesting(const UObject* WorldContextObject);

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

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "The Steam Voice speakers that play the player right now: their pawn's Owner speakers, or the 2D speaker while they have no pawn (Pawnless Voice 2D). Empty until their voice arrived. Steam voice pipeline only."))
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

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "Server mute (Server Authority in the Voice settings): off closes the player's microphone and everyone drops their voice. Kept while the server runs, also when the player leaves and joins again. Call it on the server; clients are refused (Steam.Error.WrongScope)."))
	static FSteamResult ServerSetSteamPlayerCanTalk(const UObject* WorldContextObject, FSteamId Player, bool bCanTalk);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "Whether the player's voice plays on their own pawn (Server Authority). Off: only their channels play, for dead players or spectators who may still use dead chat. Call it on the server; clients are refused (Steam.Error.WrongScope)."))
	static FSteamResult ServerSetSteamPlayerCanUseOwnerVoice(const UObject* WorldContextObject, FSteamId Player, bool bCanUse);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "The channels the player may talk and listen on (Server Authority): dead chat, team radio, spectators. Not Restricted = every channel; Restricted with no channels = none. Their client does not send other channels, nobody sends them channels they may not hear, and everyone drops the rest. Call it on the server; clients are refused (Steam.Error.WrongScope)."))
	static FSteamResult ServerSetSteamPlayerChannels(const UObject* WorldContextObject, FSteamId Player, const FSteamVoiceChannelRule& Transmit, const FSteamVoiceChannelRule& Listen);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "How far every player's own voice (their pawn) carries, in cm; 0 = no limit (Server Authority). Beyond it their voice is not sent at all, so proximity stays private. Players without a pawn, or whose pawn is not net relevant, are out of range. Channels are not limited. Keep it at least the end of your voice attenuation. Call it on the server."))
	static FSteamResult ServerSetSteamOwnerVoiceMaxDistance(const UObject* WorldContextObject, float Distance);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "How far one player's own voice carries (Server Authority), for example a megaphone power-up: above 0 replaces the game's distance for their voice, below 0 = no limit, 0 = the game's distance again. Call it on the server."))
	static FSteamResult ServerSetSteamPlayerOwnerVoiceMaxDistance(const UObject* WorldContextObject, FSteamId Player, float Distance);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "The game's owner voice distance in cm as applied here (0 = no limit). A player's own distance in their rules replaces it for their voice."))
	static float GetSteamOwnerVoiceMaxDistance(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "How voice travels (Server Authority; Topology in the Voice settings is the start value). Peer To Peer: every player sends to every other player. Server Relay: clients send to this host only and it forwards what the rules allow, so channels and proximity stay private even against cheaters working together; one extra hop, and the host uploads every forwarded stream. Clients follow with the next rules. Call it on the server; clients are refused (Steam.Error.WrongScope)."))
	static FSteamResult ServerSetSteamVoiceTopology(const UObject* WorldContextObject, ESteamVoiceTopology Topology);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "How voice travels as applied here: the server's choice (Peer To Peer until the server's rules arrived)."))
	static ESteamVoiceTopology GetSteamVoiceTopology(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "True when this game forwards everybody's voice (the Server Relay host). It keeps forwarding while its own player has voice off."))
	static bool IsSteamVoiceRelayHost(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "The server's voice rules for the player (Server Authority). Every client can read them: show a 'muted by the host' icon with Can Talk. Default rules (everything allowed) when none are set."))
	static FSteamVoicePlayerRules GetSteamPlayerVoiceRules(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "True when Server Authority is on and this game is the server (listen host or offline): the Server Set functions and Set Steam Voice Allowed work here."))
	static bool HasSteamVoiceAuthority(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice|Server Rules", meta = (WorldContext = "WorldContextObject", ToolTip = "True while Server Authority is on and the server's voice rules have not arrived yet (right after joining or travel). Nothing is sent or played meanwhile; On Voice Rules Changed fires when they arrive."))
	static bool IsSteamVoiceWaitingForRules(const UObject* WorldContextObject);
};
