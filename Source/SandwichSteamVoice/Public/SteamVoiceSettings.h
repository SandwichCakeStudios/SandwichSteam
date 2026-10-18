// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamVoiceTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "SteamVoiceSettings.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundClass;
class USoundMix;
class USteamVoiceComponent;

/** Project Settings > Plugins > Sandwich Steam - Voice */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Voice"))
class SANDWICHSTEAMVOICE_API USteamVoiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamVoiceSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "Voice is on when the game starts. Turn off to enable it from your options menu (Set Voice Enabled). While voice is off nothing runs: no microphone, no polling, no input binding."))
	bool bEnabledByDefault = true;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "On: the Steam voice pipeline (Steam capture and codec over Steam networking messages; clearer and louder). Off: the Online Subsystem voice (IOnlineVoice over the game connection) as a fallback. Changing it offers Configure Steam, which removes (on) or adds (off) [OnlineSubsystem] bHasVoiceEnabled and [Voice] bEnabled in DefaultEngine.ini. Restart the editor afterwards."))
	bool bUseSteamVoicePipeline = true;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "How the microphone opens when the game starts. The player can switch at runtime with Set Voice Mode."))
	ESteamVoiceMode DefaultMode = ESteamVoiceMode::PushToTalk;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "Mute players you have blocked on Steam as soon as they join the game."))
	bool bAutoMuteBlockedPlayers = true;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0.02", ClampMax = "1.0", ToolTip = "Seconds between checks of who is talking. Only runs while voice is enabled. 0.1 is smooth enough for a talking indicator."))
	float TalkingPollSeconds = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "Steam mutes the player's Steam friends voice chat while they talk in game (SetInGameVoiceSpeaking). Off by default because it changes the player's Steam chat; offer it in your options menu. Steam voice pipeline only."))
	bool bMuteSteamFriendsVoiceWhileTalking = false;

	UPROPERTY(Config, EditAnywhere, Category = "Microphone", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMax = "0.2", ToolTip = "Voice activation: the microphone only sends while its level is at least this loud, so breathing, keyboard and room noise stay quiet. Steam has no input gain; this is the sensitivity control. 0 = off (Steam's own silence detection only). The level is the loudness of the audio (RMS): 0 is silence, 1 the loudest possible signal. Speech is usually 0.03 to 0.2, so the slider stops at 0.2; type a higher value if you need one. Steam.Voice.Threshold shows the live level and sets the value for testing. The player can change it at runtime with Set Voice Activation Threshold. Steam voice pipeline only."))
	float VoiceActivationThreshold = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Microphone", AdvancedDisplay, meta = (ClampMin = "0", ClampMax = "2000", ForceUnits = "ms", ToolTip = "How long the microphone keeps sending after the level fell below the voice activation threshold, so word ends and short pauses are kept."))
	int32 VoiceActivationHoldMs = 300;

	UPROPERTY(Config, EditAnywhere, Category = "Pipeline", meta = (ClampMin = "0", ClampMax = "255", ToolTip = "Steam networking messages channel used for voice. Change it only when your game uses SteamNetworkingMessages on channel 7 itself."))
	int32 SteamMessageChannel = 7;

	UPROPERTY(Config, EditAnywhere, Category = "Pipeline", meta = (ClampMin = "20", ClampMax = "500", ForceUnits = "ms", ToolTip = "Audio each speaker keeps buffered before it plays. Steam hands out captured voice unevenly (up to about 160 ms at once), so below about 200 ms voice can stutter; network jitter comes on top. Lower is more direct."))
	int32 PlayoutDelayMs = 200;

	UPROPERTY(Config, EditAnywhere, Category = "Pipeline", meta = (ClampMin = "0", ClampMax = "48000", ToolTip = "Sample rate voice is decoded at. 0 uses Steam's optimal rate for the codec (recommended); 48000 is the fallback if voice sounds wrong. Steam decodes 11025 to 48000 Hz; values below 11025 use 11025."))
	int32 DecodeSampleRate = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Pipeline", AdvancedDisplay, meta = (ClampMin = "10", ClampMax = "500", ToolTip = "Most voice packets per second accepted from one player. More are dropped; a player over the limit for 5 seconds in a row is muted for the session."))
	int32 MaxPacketsPerSecond = 60;

	UPROPERTY(Config, EditAnywhere, Category = "Pipeline", AdvancedDisplay, meta = (ClampMin = "1024", ClampMax = "65536", ToolTip = "Most voice bytes per second accepted from one player (Steam voice is about 3 KB/s)."))
	int32 MaxBytesPerSecond = 8192;

	UPROPERTY(Config, EditAnywhere, Category = "Channels", meta = (Categories = "Steam.Voice.Channel", ClampMin = "0", ForceInlineRow, ToolTip = "Gives each voice channel tag (Steam.Voice.Channel.*) the id it travels as. Players talk and listen on the id, so every build that plays together needs the same map. A tag that is not in this map is refused (transmit) or stays silent (speakers). Two tags with one id are the same channel. Id channels (no tag) need no entry. Validate Project lists unmapped tags and shared ids."))
	TMap<FGameplayTag, int32> ChannelTagMap;

	/** The id Channel plays on (Channel Tag Map for tags). INDEX_NONE when unset, unmapped or negative. */
	int32 ResolveChannelId(const FSteamVoiceChannel& Channel) const { return Channel.Resolve(ChannelTagMap); }

	UPROPERTY(Config, EditAnywhere, Category = "Speakers", meta = (ToolTip = "A talker's pawn without an Owner Steam Voice component gets one, attached to the pawn's root with absolute rotation, so voice comes from the player without any setup. Turn off to place your own speakers; talkers without one are then not heard while they have a pawn. Channel speakers are never added. Steam voice pipeline only."))
	bool bAutoAddVoiceComponent = true;

	UPROPERTY(Config, EditAnywhere, Category = "Speakers", meta = (ToolTip = "The component class the plugin adds to pawns (Auto Add Voice Component) and uses for players without a pawn (menus, lobby, spectators; unattached and always 2D). Make a Blueprint child of Steam Voice to give it an attenuation, concurrency and effects. Empty = Steam Voice (2D, no attenuation)."))
	TSoftClassPtr<USteamVoiceComponent> VoiceComponentClass;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "The Enhanced Input action that is held to talk (a digital action). The plugin binds it on the local player controller's Enhanced Input component: pressed = start talking, released = stop. Leave empty to drive Start Talking / Stop Talking yourself."))
	TSoftObjectPtr<UInputAction> PushToTalkAction;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "Optional mapping context that maps the push to talk action to a key. The plugin adds it to the local player, so you do not have to. Leave empty when your own contexts already contain the action."))
	TSoftObjectPtr<UInputMappingContext> PushToTalkContext;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "Priority of the push to talk mapping context."))
	int32 PushToTalkContextPriority = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "Bind the push to talk action automatically once the local player controller has an Enhanced Input component (checked after every map load). Turn off to call Bind Steam Push To Talk yourself."))
	bool bAutoBindPushToTalk = true;

	UPROPERTY(Config, EditAnywhere, Category = "Volume", meta = (ToolTip = "The sound class the voice audio plays through. Every Steam Voice speaker without its own sound class gets this one. Needed for Set Master Volume."))
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Volume", meta = (ToolTip = "A sound mix (can be empty) the plugin uses to change the volume of the voice sound class at runtime."))
	TSoftObjectPtr<USoundMix> VoiceSoundMix;

	UPROPERTY(Config, EditAnywhere, Category = "Volume", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Master volume of the voice sound class when the game starts (1 = unchanged)."))
	float DefaultMasterVolume = 1.0f;
};
