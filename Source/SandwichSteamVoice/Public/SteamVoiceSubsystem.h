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
 *   (added when the pawn has none), or one unattached 2D speaker while the talker has no pawn.
 * - Online Subsystem voice, the fallback ([OnlineSubsystem] bHasVoiceEnabled=true and [Voice] bEnabled=true): the engine carries voice
 *   over the game connection (IOnlineVoice). [/Script/Engine.GameSession] bRequiresPushToTalk=true keeps the engine from opening the microphone.
 * When the ini does not match the setting, the path the ini allows runs and an error is logged.
 *
 * Both work while players are in the same game session. The feature adds a mode (push to talk / open mic) you can switch at runtime,
 * push to talk through Enhanced Input, mute by Steam ID with automatic muting of players you blocked on Steam, talking events and a master volume.
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

	/** Steam ID of the local player. Invalid until Steam answered. */
	FSteamId GetLocalPlayerId() const { return LocalId; }

	/** The players of the current game as of the last poll (the local player included). Empty while voice is disabled. */
	TArray<FSteamId> GetPlayersInGame() const;

	/** True while the player talks (the local player too). Read at the poll rate of the settings. */
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

	UPROPERTY(BlueprintAssignable, Category = "Steam|Voice", meta = (ToolTip = "Called when a player was muted or unmuted, also when a blocked Steam player was muted automatically."))
	FSteamVoiceMuteChanged OnPlayerMuteChanged;

	/** The speakers that play the player right now (their pawn's Owner speakers, or the 2D speaker while they have no pawn). Steam voice pipeline only. */
	TArray<USteamVoiceComponent*> GetPlayerVoiceComponents(FSteamId Player) const;

	/** A Steam Voice speaker began play, switched on or off (called by the component). */
	void RegisterSpeaker(USteamVoiceComponent* Speaker);

	/** A Steam Voice speaker ended play (called by the component). */
	void UnregisterSpeaker(USteamVoiceComponent* Speaker);

	/** A speaker was switched on or off, or changed channel: route again on the next tick (called by the component). */
	void MarkSpeakersDirty() { bSpeakersDirty = true; }

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

	/** Handles the worker's events: speakers, talking. Runs every frame while the pipeline is up. */
	void DrainWorkerEvents();

	struct FSpeakerSource
	{
		TSharedPtr<Audio::FPatchSplitter, ESPMode::ThreadSafe> Splitter;
		int32 SampleRate = 0;
		bool bFake = false;
		bool bActive = false;
		/** The talker's pawn at the last speaker update (nullptr: no pawn, the 2D speaker plays). */
		TWeakObjectPtr<APawn> Pawn;
		/** Speakers that read a patch of Splitter. */
		TArray<TWeakObjectPtr<USteamVoiceComponent>> Connected;
	};

	/** Connects every talker to its pawn's Owner speakers (auto-adding one), or to a 2D speaker without a pawn. Runs when bSpeakersDirty. */
	void UpdateSpeakers();
	void ConnectSpeakers(int64 Talker, FSpeakerSource& Source, TConstArrayView<USteamVoiceComponent*> Wanted);
	void DisconnectSpeakers(int64 Talker, FSpeakerSource& Source);
	USteamVoiceComponent* AddAutoSpeaker(int64 Talker, APawn* Pawn);
	USteamVoiceComponent* EnsureFallbackSpeaker(int64 Talker);
	void DestroyPluginSpeaker(TMap<int64, TObjectPtr<USteamVoiceComponent>>& Map, int64 Talker);
	/** Disconnects all talkers and destroys the plugin-created speakers (world change, pipeline stop). */
	void DestroySpeakers();
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

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

	/** Talkers the worker knows (connected patch splitter), whether their speaker plays, who talks (worker events). */
	TMap<int64, FSpeakerSource> SpeakerSources;
	TSet<int64> PipelineTalking;

	/** Speakers that play in a world, switched on or off (developer-placed and auto-added). */
	TSet<TWeakObjectPtr<USteamVoiceComponent>> RegisteredSpeakers;

	/** Plugin-created speakers of the current world: added to a talker's pawn, or unattached 2D while the talker has no pawn. Cleared before the world is destroyed. */
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<USteamVoiceComponent>> AutoSpeakers;
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<USteamVoiceComponent>> FallbackSpeakers;
	TWeakObjectPtr<UWorld> SpeakerWorld;
	/** Talker player states (from the player poll), for talker -> pawn. Their OnPawnSet is bound. */
	TMap<int64, TWeakObjectPtr<APlayerState>> PlayerStates;
	/** Speakers, pawns or talkers changed: run UpdateSpeakers on the next tick. */
	bool bSpeakersDirty = false;
	FDelegateHandle WorldCleanupHandle;

	/** What the worker was last sent, to publish only changes. */
	TArray<uint64> PublishedPeers;
	TArray<uint64> PublishedMuted;
	TArray<uint64> PublishedBlocked;
	int64 PublishedLocalId = 0;
	bool bPublishedRecord = false;
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
	ESteamVoiceMode Mode = ESteamVoiceMode::PushToTalk;
	/** Push to talk held, or Start Talking called. */
	bool bTalkRequested = false;
	/** The microphone state last given to the Steam pipeline or the Online Subsystem fallback. */
	bool bMicOpen = false;
	float MasterVolume = 1.0f;
	float ActivationThreshold = 0.0f;

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
