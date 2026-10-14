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
class APlayerController;
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
 * Two pipelines, picked once when the feature starts:
 * - Steam voice pipeline (when the Online Subsystem has no voice interface): Steam capture and codec, SteamNetworkingMessages between the
 *   players of the game, a voice worker thread (jitter, decode), and USteamVoiceComponent speakers (one 2D speaker per talker for now).
 * - Online Subsystem voice (when [OnlineSubsystem] bHasVoiceEnabled=true and [Voice] bEnabled=true): the engine carries voice over the
 *   game connection (IOnlineVoice). Needs [/Script/Engine.GameSession] bRequiresPushToTalk=true as well.
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

	/** True when a headset or microphone is present. */
	bool IsHeadsetPresent() const;

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

	USteamVoiceComponent* EnsureSpeaker(int64 Talker);
	void DestroySpeakers();
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	struct FSpeakerSource
	{
		TSharedPtr<Audio::FPatchSplitter, ESPMode::ThreadSafe> Splitter;
		int32 SampleRate = 0;
		bool bFake = false;
		bool bActive = false;
	};

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

	/** Plugin-created speakers of the current world. Cleared before the world is destroyed. */
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<USteamVoiceComponent>> Speakers;
	TWeakObjectPtr<UWorld> SpeakerWorld;
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
