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
class FSteamVoiceBackend;
class FUniqueNetId;
class UEnhancedInputComponent;
class ULocalPlayer;
class UInputMappingContext;
class USoundMix;
class UWorld;

/**
 * Steam in-session voice chat. Client only (the server relays voice by itself).
 *
 * The engine's Steam Online Subsystem carries the voice over the game connection (IOnlineVoice), so voice works while players are in
 * the same game session: a listen server, a dedicated server or a Steam lobby that travelled to a map. Chat in a lobby before the travel
 * is not supported. The feature adds what the Online Subsystem lacks: a mode (push to talk / open mic) you can switch at runtime, push to talk
 * through Enhanced Input, mute by Steam ID with automatic muting of players you blocked on Steam, talking events for a UI, and a master volume.
 *
 * Needs [OnlineSubsystem] bHasVoiceEnabled=true and [/Script/Engine.GameSession] bRequiresPushToTalk=true (Tools > Sandwich Steam > Configure Steam
 * writes both). The ini always asks for push to talk; the feature opens the microphone itself in open mic mode, so the mode can change at runtime.
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

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Voice.Dump. */
	FString BuildDebugString() const;
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

	/** Sends the wanted mute state of one player to the backend when it is not confirmed yet. */
	void ApplyMute(FSteamId Player, const TSharedPtr<const FUniqueNetId>& NetId);

	void BroadcastTalking(int64 Player, bool bTalking);

	TSharedPtr<FSteamVoiceBackend> Backend;
	TUniquePtr<SandwichSteam::Voice::FTalkingTracker> Tracker;
	TUniquePtr<SandwichSteam::Voice::FMuteBook> Book;

	FTSTicker::FDelegateHandle TickHandle;

	bool bEnabled = false;
	ESteamVoiceMode Mode = ESteamVoiceMode::PushToTalk;
	/** Push to talk held, or Start Talking called. */
	bool bTalkRequested = false;
	/** What the backend was last told. */
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
