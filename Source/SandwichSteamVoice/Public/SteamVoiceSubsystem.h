// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "SteamVoiceRules.h"
#include "SteamVoiceTypes.h"
#include "SteamVoiceSubsystem.generated.h"

class AGameStateBase;
class APawn;
class APlayerController;
class APlayerState;
class ASteamVoiceAuthority;
class FSteamVoiceOssFallback;
class FSteamVoiceTransport;
class FSteamVoiceWorker;
class FUniqueNetId;
class UEnhancedInputComponent;
class ULocalPlayer;
class UInputMappingContext;
class USoundMix;
class USteamVoiceComponent;
class UWorld;

namespace Audio
{
	class FPatchSplitter;
}

/**
 * Steam in-session voice chat. Client only.
 *
 * Two pipelines, picked once when the feature starts by Use Steam Voice Pipeline (Configure Steam writes the matching ini):
 * - Steam voice pipeline (default; the Online Subsystem has no voice interface): Steam capture and codec, SteamNetworkingMessages between the
 *   players of the game, a voice worker thread (jitter, decode), and USteamVoiceComponent speakers: the talker's pawn speakers
 *   (added when the pawn has none), or while the talker has no pawn one unattached 2D speaker or silence (Set Pawnless Voice).
 * - Online Subsystem voice, the fallback ([OnlineSubsystem] bHasVoiceEnabled=true and [Voice] bEnabled=true): the engine carries voice
 *   over the game connection (IOnlineVoice). [/Script/Engine.GameSession] bRequiresPushToTalk=true keeps the engine from opening the microphone.
 * When the ini does not match the setting, the path the ini allows runs and an error is logged.
 *
 * Both work while players are in the same game session. The feature adds a mode (push to talk / open mic) you can switch at runtime,
 * push to talk through Enhanced Input, mute by Steam ID with automatic muting of players you blocked on Steam, talking events and a master volume.
 *
 * With Server Authority (Voice settings) the server decides the voice rules: an ASteamVoiceAuthority replicates them to every client, which
 * follows them on both ends and sends and plays nothing until they arrived.
 *
 * While voice is disabled (Set Voice Enabled false, or Enabled By Default off) nothing runs: no polling, no input binding, no microphone.
 * All methods run on the game thread.
 */
UCLASS()
class SANDWICHSTEAMVOICE_API USteamVoiceSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Voice subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamVoiceSubsystem* Get(const UObject* WorldContext);

	USteamVoiceSubsystem();
	virtual ~USteamVoiceSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** Turns voice on or off. Off closes the microphone, stops the talking checks and removes the push to talk binding. */
	FSteamResult SetVoiceEnabled(bool bEnabled);
	bool IsVoiceEnabled() const { return bEnabled; }

	/**
	 * The game's voice rule, separate from the player's Set Voice Enabled option (voice runs while both are on): forbid voice in a lobby, allow
	 * it in the match. Not allowed: the microphone closes, nothing is sent and other players are not played or reported talking; the mic test
	 * and push to talk binding stay, and a held push to talk opens the microphone once allowed. Without Server Authority it is local to this
	 * client: call it on every client (from replicated game state). With Server Authority call it on the server; it reaches every client with
	 * the rules, and clients are refused (WrongScope). Online Subsystem voice: only closes the microphone.
	 */
	FSteamResult SetVoiceAllowed(bool bAllowed);
	bool IsVoiceAllowed() const { return bVoiceAllowed; }

	/**
	 * How players are heard while they have no pawn (menus, lobby, spectating, between death and respawn): one 2D speaker, or silent until they
	 * have a pawn again. Applies on the next speaker update. Fake talkers and the mic test always play 2D. With Server Authority call it on the
	 * server (clients are refused, WrongScope). Steam voice pipeline only.
	 */
	FSteamResult SetPawnlessVoice(ESteamVoicePawnlessMode NewMode);
	ESteamVoicePawnlessMode GetPawnlessVoice() const { return PawnlessVoice; }

	/** Switches between push to talk and open mic. Works while voice is disabled too (the mode is used when it is enabled). */
	FSteamResult SetVoiceMode(ESteamVoiceMode NewMode);
	ESteamVoiceMode GetVoiceMode() const { return Mode; }

	/** Opens the microphone (push to talk mode). Call it from your own input handling; not needed when the push to talk action is set in the settings. */
	FSteamResult StartTalking();

	/** Closes the microphone (push to talk mode). In open mic mode the microphone stays open. */
	FSteamResult StopTalking();

	/** True while the local microphone is open and picks up voice. */
	bool IsLocalTalking() const;

	/**
	 * True when a headset or microphone is present, as far as Steam tells. Not reliable with the Steam voice pipeline: Steam reports
	 * a missing microphone like silence, so this is only false when Steam's voice capture is not initialized. Show the local mic level
	 * (Get Local Mic Level) while the player talks to let them see that their microphone works.
	 */
	bool IsHeadsetPresent() const;

	/**
	 * Voice activation level (RMS, 0 to 1; 0 = off): captured audio below it is not sent. Steam has no input gain, so this is the
	 * microphone sensitivity of your options menu. Kept for the life of the game instance. Steam voice pipeline only.
	 */
	FSteamResult SetVoiceActivationThreshold(float Threshold);
	float GetVoiceActivationThreshold() const { return ActivationThreshold; }

	/** Level (RMS, 0 to 1) of the local microphone while it is open and picks up sound, else 0. Same scale as the activation threshold. Steam voice pipeline only. */
	float GetLocalMicLevel() const;

	/**
	 * Mic test for an options menu: opens the microphone and plays the local voice back on its own 2D speaker, one playout delay late and after
	 * voice activation, so the player hears what others would hear. Nothing is sent to other players unless they also talk (push to talk held,
	 * open mic). Needs voice enabled; ends when voice is disabled. Steam voice pipeline only.
	 */
	FSteamResult StartMicTest();
	FSteamResult StopMicTest();
	bool IsMicTesting() const { return bMicTest; }

	/**
	 * The channels the local player talks on (walkie, radio, PA), at most 8. Everyone's Channel speakers on these channels play the local voice.
	 * Applies to the next captured packet, so the radio starts and stops on the right syllable. Kept for the life of the game instance.
	 * Invalid channels (neither a tag nor an id, or a tag missing from Channel Tag Map) are refused. Steam voice pipeline only.
	 */
	FSteamResult SetTransmitChannels(const TArray<FSteamVoiceChannel>& Channels);
	FSteamResult AddTransmitChannel(const FSteamVoiceChannel& Channel);
	FSteamResult RemoveTransmitChannel(const FSteamVoiceChannel& Channel);
	FSteamResult ClearTransmitChannels();
	const TArray<FSteamVoiceChannel>& GetTransmitChannels() const { return TransmitChannels; }
	bool IsTransmittingOn(const FSteamVoiceChannel& Channel) const { return TransmitChannels.Contains(Channel); }

	/**
	 * The channels a player talks on right now (receiver side): what their voice carries while they talk, empty while they do not, or are
	 * muted (unless Report Muted Talking). The local player gets what the local voice is sent on. An id with tags in Channel Tag Map comes
	 * back as those tags (one entry per tag), else as the id. Steam voice pipeline only.
	 */
	TArray<FSteamVoiceChannel> GetPlayerTransmitChannels(FSteamId Player) const;

	/** True while the player talks on the channel (tags compare by their id, so two tags sharing an id are the same channel). */
	bool IsPlayerTransmittingOn(FSteamId Player, const FSteamVoiceChannel& Channel) const;

	/** Whether the local voice plays on the local player's own pawn (Owner speakers, proximity). Default true; false = radio only. Steam voice pipeline only. */
	FSteamResult SetTransmitToOwner(bool bTransmit);
	bool IsTransmittingToOwner() const { return bTransmitToOwner; }

	/** Steam ID of the local player. Invalid until Steam answered. */
	FSteamId GetLocalPlayerId() const { return LocalId; }

	/** The players of the current game as of the last poll (the local player included). Empty while voice is disabled. */
	TArray<FSteamId> GetPlayersInGame() const;

	/**
	 * True while the player talks (the local player too). Steam voice pipeline: from the voice itself, at once (a muted player only with
	 * Report Muted Talking). Online Subsystem voice: read at Talking Poll Seconds.
	 */
	bool IsPlayerTalking(FSteamId Player) const;
	TArray<FSteamId> GetTalkingPlayers() const;

	/**
	 * Mutes a player's voice for this game. The mute is kept for the life of the game instance and applies as soon as the player is in the game.
	 * bSystemWide asks the Online Subsystem for a mute that also affects other games (the Steam Online Subsystem treats it like a game mute).
	 */
	FSteamResult MutePlayer(FSteamId Player, bool bSystemWide = false);

	/** Removes the mute you set. A player you blocked on Steam stays muted while the automatic mute is on. */
	FSteamResult UnmutePlayer(FSteamId Player);
	bool IsPlayerMuted(FSteamId Player) const;

	/** Everybody who is muted, by you or automatically. */
	TArray<FSteamId> GetMutedPlayers() const;

	/** Volume of the voice sound class (Voice settings), 0 to 2. Needs a sound class in the settings. */
	FSteamResult SetMasterVolume(float Volume);
	float GetMasterVolume() const { return MasterVolume; }

	/**
	 * Volume of one player's voice for the local player, 0 to 2 (1 = unchanged). 0 silences them without a mute: their talking indicator still works.
	 * Applied on the voice thread before mixing, so it covers their Owner speakers and every channel they talk on; a change fades over one packet.
	 * Kept for the life of the game instance and applies as soon as the player is in the game. Steam voice pipeline only.
	 */
	FSteamResult SetPlayerVolume(FSteamId Player, float Volume);

	/** The volume set for the player, 1 when none was set. */
	float GetPlayerVolume(FSteamId Player) const;

	/**
	 * Level (RMS, 0 to 1, same scale as the local mic level) of the player's voice as it plays, before their player volume; 0 while they
	 * do not talk or are muted. The local player gets the local mic level. Drive a per player meter with it. Steam voice pipeline only.
	 */
	float GetPlayerVoiceLevel(FSteamId Player) const;

	/**
	 * Binds the push to talk action of the settings on the controller's Enhanced Input component (and adds the mapping context of the settings).
	 * The plugin does this by itself after every map load when Auto Bind Push To Talk is on; call it yourself when it is off, or for split screen.
	 */
	FSteamResult BindPushToTalk(APlayerController* Controller);

	/** True while the push to talk action is bound. */
	bool IsPushToTalkBound() const;

	/** Called when a player (or the local player) starts or stops talking. Fires on the game thread. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when a player or the local player starts or stops talking. Drive a talking indicator with it."))
	FSteamVoiceTalkingChanged OnPlayerTalkingChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when the voice mode changed."))
	FSteamVoiceModeChanged OnVoiceModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when the game allowed or forbade voice (Set Voice Allowed). Show 'voice chat starts with the match' with it."))
	FSteamVoiceAllowedChanged OnVoiceAllowedChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when a player was muted or unmuted, also when a blocked Steam player was muted automatically."))
	FSteamVoiceMuteChanged OnPlayerMuteChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when the channels a player talks on changed: they started or stopped talking on a radio, or switched channels. Empty once they stopped. Also for the local player. Drive a 'Blue is on the radio' indicator with it. Steam voice pipeline only."))
	FSteamVoiceTransmitChannelsChanged OnPlayerTransmitChannelsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when a player sent too much voice data for 5 seconds in a row; they are muted for this session. Kick or report them if your game wants to. Steam voice pipeline only."))
	FSteamVoicePlayerFlood OnPlayerVoiceFlood;

	/** The speakers that play the player right now (their pawn's Owner speakers, or the 2D speaker while they have no pawn and Pawnless Voice is 2D). Steam voice pipeline only. */
	TArray<USteamVoiceComponent*> GetPlayerVoiceComponents(FSteamId Player) const;

	/** A Steam Voice speaker began play, switched on or off (called by the component). */
	void RegisterSpeaker(USteamVoiceComponent* Speaker);

	/** A Steam Voice speaker ended play (called by the component). */
	void UnregisterSpeaker(USteamVoiceComponent* Speaker);

	/** A speaker was switched on or off, or changed channel: route again on the next tick (called by the component). */
	void MarkSpeakersDirty() { bSpeakersDirty = true; }

	/**
	 * The players a speaker plays right now (called by the component): an Owner speaker its talker while they talk on their pawn, a Channel
	 * speaker everyone talking on its channel (minus its owner unless Include Owner). Muted players never.
	 */
	TArray<FSteamId> GetSpeakerTalkers(const USteamVoiceComponent* Speaker) const;

	// ---- Server rules (Server Authority in the Voice settings) ----

	/** True when Server Authority is on and the Steam voice pipeline is used: the server decides the voice rules. */
	bool IsServerAuthorityEnabled() const { return bServerAuthority; }

	/** True when Server Authority is on and this game instance is the server (listen host or standalone): the Server Set functions work here. */
	bool HasVoiceAuthority() const;

	/** True while Server Authority is on and the server's rules have not arrived (joining, travel): nothing is sent or played. */
	bool IsWaitingForVoiceRules() const { return bServerAuthority && !bRulesReceived; }

	/** The version of the rules last applied here (0 = none yet). Rises with every change on the server. */
	int32 GetVoiceRulesVersion() const { return AppliedRulesVersion; }

	/**
	 * Server mute: the player's microphone closes and everyone drops their voice. Kept for the life of the server's game instance, also
	 * when the player leaves and joins again. Server only (clients: WrongScope); needs Server Authority.
	 */
	FSteamResult ServerSetPlayerCanTalk(FSteamId Player, bool bCanTalk);

	/** Whether the player's voice may play on their own pawn (off: only their channels, e.g. dead chat). Server only; needs Server Authority. */
	FSteamResult ServerSetPlayerCanUseOwnerVoice(FSteamId Player, bool bCanUse);

	/**
	 * The channels the player may talk and listen on (dead chat, team radio, spectators). Not restricted = every channel; restricted with an
	 * empty list = none. Their client does not send other channels, nobody sends them channels they may not hear, and everyone drops the rest.
	 * Channels must resolve with the server's Channel Tag Map. Server only; needs Server Authority.
	 */
	FSteamResult ServerSetPlayerChannels(FSteamId Player, const FSteamVoiceChannelRule& Transmit, const FSteamVoiceChannelRule& Listen);

	/**
	 * How far every player's own voice (their pawn) carries, in cm; 0 = no limit. Beyond it their voice is not sent; a listener whose pawn or the
	 * talker's pawn is unknown (no pawn, not net relevant) gets no owner voice either. Channels are not limited. Keep it at least the end of
	 * your voice attenuation, and pawns net relevant that far. Server only; needs Server Authority.
	 */
	FSteamResult ServerSetOwnerVoiceMaxDistance(float Distance);

	/** One talker's own distance (megaphone): above 0 replaces the game's distance for their voice, below 0 = no limit, 0 = the game's. Server only; needs Server Authority. */
	FSteamResult ServerSetPlayerOwnerVoiceMaxDistance(FSteamId Player, float Distance);

	/** The game's owner voice distance as applied here (0 = no limit). */
	float GetOwnerVoiceMaxDistance() const { return OwnerVoiceMaxDistance; }

	/** The server's rules for the player (default rules when none are set or Server Authority is off). */
	FSteamVoicePlayerRules GetPlayerVoiceRules(FSteamId Player) const;

	/** True when the server's Channel Tag Map differs from this build's: channel rules and speakers may not match the server's channels. */
	bool IsChannelMapMismatched() const { return bChannelMapMismatch; }

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Server only (Server Authority): a player joined with a Steam ID another player already has. Their voice is refused both ways until one of them leaves. Kick the joining player if your game wants to; the plugin does not kick. Steam auth is needed to verify Steam IDs."))
	FSteamVoiceIdentityConflict OnVoiceIdentityConflict;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when the server's voice rules arrived or changed (Server Authority): after joining or travel, and whenever the server changes a rule. Refresh a 'muted by the host' icon with it."))
	FSteamVoiceRulesChanged OnVoiceRulesChanged;

	/** The authority actor of the current world began play, changed or ends play (called by ASteamVoiceAuthority). */
	void HandleAuthorityChanged(ASteamVoiceAuthority* InAuthority, bool bRemoved);

	/** True when the Steam voice pipeline runs (false: Online Subsystem voice, or voice disabled). */
	bool IsUsingSteamPipeline() const { return bUseSteamPipeline; }

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Voice.Dump. */
	FString BuildDebugString() const;

	/** Steam.Voice.FakeTalker: re-inject the local capture as Count fake talkers (0 = off), fake i DelayMs + i * StaggerMs late. Steam voice pipeline only. */
	FSteamResult SetFakeTalkers(int32 Count, int32 DelayMs, float LossPercent, int32 StaggerMs, int32 Channel);

	/** Steam.Voice.SimLoss: drop this percent of real received voice packets. Steam voice pipeline only. */
	FSteamResult SetSimulatedLoss(float LossPercent);
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	bool TickVoice(float DeltaTime);

	/** Puts the runtime state in line with bEnabled: ticker, microphone, binding, volume. */
	void ApplyEnabledState();

	/** Opens or closes the microphone so it matches ShouldTalk(). bForce sends the call even when the state did not change. */
	void RefreshTalking(bool bForce);
	bool ShouldTalk() const;

	void HandlePushToTalkPressed();
	void HandlePushToTalkReleased();

	bool TryBindPushToTalk(APlayerController* Controller);
	void UnbindPushToTalk();

	bool ApplyMasterVolume();
	void RemoveVolumeMix();

	/** Sends the wanted mute state of one player to the Online Subsystem fallback when it is not confirmed yet. */
	void ApplyMute(FSteamId Player, const TSharedPtr<const FUniqueNetId>& NetId);

	void BroadcastTalking(int64 Player, bool bTalking);

	// ---- Steam voice pipeline ----

	/** Starts the transport and the worker (voice enabled). False when another game instance in this process owns the microphone. */
	bool StartPipeline();
	void StopPipeline();

	/** Sends the current routing (players, mutes, microphone) to the worker when it changed. */
	void PublishRouting();

	/** Shared checks of the transmit setters: feature active, Steam pipeline. */
	bool CanSetTransmit(FSteamResult& OutResult) const;

	/** TransmitChannels as packet ids: resolved, unique, sorted. Unresolved channels are left out. */
	TArray<int32> ResolveTransmitChannels() const;

	/** Handles the worker's events: speakers, talking. Runs every frame while the pipeline is up. */
	void DrainWorkerEvents();

	struct FSpeakerSource
	{
		TSharedPtr<Audio::FPatchSplitter, ESPMode::ThreadSafe> Splitter;
		int32 SampleRate = 0;
		bool bFake = false;
		bool bActive = false;
		/** The talker's pawn at the last speaker update (nullptr: no pawn, the 2D speaker plays or the talker is silent). */
		TWeakObjectPtr<APawn> Pawn;
		/** Speakers that read a patch of Splitter. */
		TArray<TWeakObjectPtr<USteamVoiceComponent>> Connected;
	};

	/** One channel mix: everyone on Channel minus the player Excluded (0 = the full mix). */
	struct FChannelOutputKey
	{
		int32 Channel = INDEX_NONE;
		int64 Excluded = 0;

		bool operator==(const FChannelOutputKey& Other) const { return Channel == Other.Channel && Excluded == Other.Excluded; }
		friend uint32 GetTypeHash(const FChannelOutputKey& Key) { return HashCombine(::GetTypeHash(Key.Channel), ::GetTypeHash(Key.Excluded)); }
	};

	struct FChannelOutputState
	{
		/** Created here, filled by the worker. */
		TSharedPtr<Audio::FPatchSplitter, ESPMode::ThreadSafe> Splitter;
		bool bActive = false;
		/** Channel speakers that read a patch of Splitter. */
		TArray<TWeakObjectPtr<USteamVoiceComponent>> Connected;
	};

	/**
	 * Connects every talker to its pawn's Owner speakers (auto-adding one), or without a pawn to a 2D speaker or nothing (PawnlessVoice), and every switched on
	 * Channel speaker to its channel mix. Runs when bSpeakersDirty.
	 */
	void UpdateSpeakers();
	/** Channel speakers: one mix per channel and excluded owner; publishes the routing when the set of mixes changed. */
	void UpdateChannelSpeakers(UWorld* World);
	void ConnectSpeakers(int64 Talker, FSpeakerSource& Source, TConstArrayView<USteamVoiceComponent*> Wanted);
	void DisconnectSpeakers(int64 Talker, FSpeakerSource& Source);
	USteamVoiceComponent* AddAutoSpeaker(int64 Talker, APawn* Pawn);
	USteamVoiceComponent* EnsureFallbackSpeaker(int64 Talker);
	void DestroyPluginSpeaker(TMap<int64, TObjectPtr<USteamVoiceComponent>>& Map, int64 Talker);
	/** Disconnects all talkers and destroys the plugin-created speakers (world change, pipeline stop). */
	void DestroySpeakers();
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	// ---- Server rules ----

	/** True when the world of this game instance is not a network client. */
	bool IsServerWorld() const;

	/** Uses the authority actor already in the world (feature started late), or on the server spawns one. World: a world of this game instance. */
	void EnsureAuthority(UWorld* World);

	/** Server: the shared checks of the Server Set functions. */
	bool CanSetServerRules(FSteamResult& OutResult) const;

	/** Server: one player's rules changed; pushes them to the authority and applies them here. */
	void SetServerPlayerRules(FSteamId Player, const FSteamVoicePlayerRules& Rules);

	/** Server: a player's rules as the authority carries them (the game's, plus an identity conflict). */
	FSteamVoicePlayerRules GetEffectiveServerRules(int64 Player) const;

	/** Server: pushes one player's effective rules to the authority and applies them here. */
	void PushServerPlayerRules(int64 Player);

	/** Server: Ids are the Steam IDs of every player state in order (the joining player last); refuses voice for duplicates and fires the event. */
	void UpdateIdentityConflicts(TConstArrayView<int64> Ids, TConstArrayView<APlayerState*> States);

	/** Owner voice distance: which peers our mouth reaches and which talkers' mouths may play here (from the pawns, every poll). */
	void UpdateOwnerVoiceRange();

	/** The applied rules as the voice thread needs them (channel ids from this build's Channel Tag Map). */
	void ResolveAppliedRules();

	/** Server: the game rules changed; pushes them to the authority and applies them here. */
	void PushServerGameRules();

	/** Server: raises the version on the authority (clients see one version per change). */
	void BumpServerRulesVersion();

	/** Reads the authority actor (or its absence) into the applied rules; publishes routing and fires events when something changed. */
	void ApplyAuthorityRules();

	/** The local player may talk / use their mouth under the applied rules. */
	bool CanLocalTalkByRules() const;

	/** Voice may run under the game rules: allowed, and the server's rules arrived (Server Authority). */
	bool IsVoiceAllowedByRules() const { return bVoiceAllowed && !IsWaitingForVoiceRules(); }

	/** Remembers a talker's player state and listens to its pawn changes. */
	void TrackPlayerState(int64 Player, APlayerState* PlayerState);
	void ForgetPlayerStates();

	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	TSharedPtr<FSteamVoiceOssFallback> OssFallback;
	TSharedPtr<FSteamVoiceTransport> Transport;
	/** Shared pointers keep the forward declarations above enough for the generated code. */
	TSharedPtr<FSteamVoiceWorker> Worker;
	bool bUseSteamPipeline = false;
	/** Seconds since the last player poll (the ticker runs every frame while the pipeline is up). */
	float PollAccumulator = 0.0f;

	/** Talkers the worker knows (connected patch splitter), whether their speaker plays. */
	TMap<int64, FSpeakerSource> SpeakerSources;

	/** Where a player's voice goes while they talk (worker events). Only players who talk on their pawn or a channel. */
	struct FPlayerTransmit
	{
		bool bMouth = false;
		TArray<int32> Channels;
	};
	TMap<int64, FPlayerTransmit> PlayerTransmits;

	/** Channel ids as channels for Blueprint: the tags Channel Tag Map gives each id, else the id. */
	static TArray<FSteamVoiceChannel> MakeChannels(TConstArrayView<int32> Ids);

	/** Forgets every player's transmit channels, telling listeners (pipeline stop). */
	void ClearPlayerTransmits();

	/** Speakers that play in a world, switched on or off (developer-placed and auto-added). */
	TSet<TWeakObjectPtr<USteamVoiceComponent>> RegisteredSpeakers;

	/** Plugin-created speakers of the current world: added to a talker's pawn, or unattached 2D while the talker has no pawn. Cleared before the world is destroyed. */
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<USteamVoiceComponent>> AutoSpeakers;
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<USteamVoiceComponent>> FallbackSpeakers;
	TWeakObjectPtr<UWorld> SpeakerWorld;
	/** The channel mixes the Channel speakers of the current world play. */
	TMap<FChannelOutputKey, FChannelOutputState> ChannelOutputs;
	/** ChannelOutputs gained or lost a mix since the last routing publish. */
	bool bChannelOutputsChanged = false;
	/** Talker player states (from the player poll), for talker -> pawn. Their OnPawnSet is bound. */
	TMap<int64, TWeakObjectPtr<APlayerState>> PlayerStates;
	/** Speakers, pawns or talkers changed: run UpdateSpeakers on the next tick. */
	bool bSpeakersDirty = false;
	FDelegateHandle WorldCleanupHandle;

	/** What the worker was last sent, to publish only changes. */
	TArray<uint64> PublishedPeers;
	TArray<uint64> PublishedMuted;
	TArray<uint64> PublishedBlocked;
	TArray<int32> PublishedTransmitChannels;
	TArray<uint64> PublishedMouthNotSent;
	TArray<uint64> PublishedMouthDropped;
	int64 PublishedLocalId = 0;
	bool bPublishedRecord = false;
	bool bPublishedMicTest = false;
	bool bPublishedTransmitMouth = true;
	/** PlayerVolumes changed since the last routing publish. */
	bool bPlayerVolumesChanged = false;
	bool bRoutingPublished = false;
	int32 FakeTalkerCount = 0;
	int32 FakeTalkerDelayMs = 100;
	int32 FakeTalkerStaggerMs = 250;
	float FakeTalkerLossPercent = 0.0f;
	int32 FakeTalkerChannel = -1;
	float SimulatedLossPercent = 0.0f;
	/** Another game instance (PIE) already runs the pipeline: warned once. */
	bool bWarnedPipelineOwner = false;
	TUniquePtr<SandwichSteam::Voice::FTalkingTracker> Tracker;
	TUniquePtr<SandwichSteam::Voice::FMuteBook> Book;

	FTSTicker::FDelegateHandle TickHandle;

	bool bEnabled = false;
	/** The game's rule as applied here (Set Voice Allowed, or the server's with Server Authority); voice runs while bEnabled and this. */
	bool bVoiceAllowed = true;
	ESteamVoicePawnlessMode PawnlessVoice = ESteamVoicePawnlessMode::Play2D;

	/** Server Authority setting and the Steam voice pipeline, fixed when the feature starts. */
	bool bServerAuthority = false;
	/** The authority actor of the current world (spawned here on the server, replicated on clients). */
	TWeakObjectPtr<ASteamVoiceAuthority> Authority;
	/** Server: what this game instance decides while it is the server. Kept across travel; a new authority starts from it. */
	bool bServerVoiceAllowed = true;
	ESteamVoicePawnlessMode ServerPawnlessVoice = ESteamVoicePawnlessMode::Play2D;
	TMap<int64, FSteamVoicePlayerRules> ServerPlayerRules;
	float ServerOwnerVoiceMaxDistance = 0.0f;
	/** Server: Steam IDs two players in the game have; their voice is refused. */
	TSet<int64> IdentityConflicts;
	int32 ServerRulesVersion = 0;
	/** The rules as applied here (from the authority actor). Only players with a restriction. */
	bool bRulesReceived = false;
	int32 AppliedRulesVersion = 0;
	TMap<int64, FSteamVoicePlayerRules> AppliedPlayerRules;
	float OwnerVoiceMaxDistance = 0.0f;
	/** AppliedPlayerRules resolved for the worker. */
	TMap<int64, SandwichSteam::Voice::FPlayerRules> ResolvedRules;
	/** Hash of this build's Channel Tag Map (the server sends its own on the authority). */
	uint32 LocalChannelMapHash = 0;
	/** The server's channel map differs from ours (warned when it starts). */
	bool bChannelMapMismatch = false;
	/** Owner voice distance: peers in range of our mouth (sender hysteresis), and the current out-of-range sets for the worker. */
	TMap<int64, bool> MouthInRange;
	TArray<uint64> MouthNotSent;
	TArray<uint64> MouthDropped;
	FDelegateHandle ActorsInitializedHandle;
	ESteamVoiceMode Mode = ESteamVoiceMode::PushToTalk;
	/** Push to talk held, or Start Talking called. */
	bool bTalkRequested = false;
	/** The microphone state last given to the Steam pipeline or the Online Subsystem fallback. */
	bool bMicOpen = false;
	/** Start Mic Test was called (records even while bMicOpen is false, plays back, sends nothing by itself). */
	bool bMicTest = false;
	float MasterVolume = 1.0f;
	float ActivationThreshold = 0.0f;
	/** What the local player transmits on (talker side). */
	TArray<FSteamVoiceChannel> TransmitChannels;
	bool bTransmitToOwner = true;
	/** Per player volume (receiver side). Only players not at 1. */
	TMap<int64, float> PlayerVolumes;

	FSteamId LocalId;

	/** Players in the game at the last poll, and the ones already checked against the Steam block list this session. */
	TSet<int64> Present;
	TSet<int64> Evaluated;
	TWeakObjectPtr<const AGameStateBase> LastGameState;

	// Push to talk binding.
	TWeakObjectPtr<UEnhancedInputComponent> BoundComponent;
	TWeakObjectPtr<ULocalPlayer> ContextPlayer;
	TWeakObjectPtr<const UInputMappingContext> AddedContext;
	uint32 PressedBindingHandle = 0;
	uint32 ReleasedBindingHandle = 0;
	uint32 CanceledBindingHandle = 0;
	/** The push to talk action of the settings could not be loaded: stop trying until voice is enabled again. */
	bool bPushToTalkUnavailable = false;

	// Volume mix.
	TWeakObjectPtr<UWorld> MixWorld;
	TWeakObjectPtr<USoundMix> PushedMix;

	// What the last poll saw in the game state (for the dump: why is a player not listed?).
	int32 SeenPlayerStates = 0;
	int32 SeenWithoutNetId = 0;
	int32 SeenNotSteamId = 0;
	FString NotSteamSample;

	// Counters for the dump.
	int32 TalkingEventsSent = 0;
	int32 MuteCallsFailed = 0;
};
