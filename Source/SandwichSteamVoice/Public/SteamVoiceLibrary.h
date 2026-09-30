// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamVoiceTypes.h"
#include "SteamVoiceLibrary.generated.h"

class APlayerController;

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

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True when a headset or microphone is present."))
	static bool IsSteamHeadsetPresent(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the player talks. Updated at the poll rate of the Voice settings."))
	static bool IsSteamPlayerTalking(const UObject* WorldContextObject, FSteamId Player);

	UFUNCTION(BlueprintPure, Category = "Steam|Voice", meta = (WorldContext = "WorldContextObject", ToolTip = "Everybody who talks right now, the local player included."))
	static TArray<FSteamId> GetSteamTalkingPlayers(const UObject* WorldContextObject);

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
};
