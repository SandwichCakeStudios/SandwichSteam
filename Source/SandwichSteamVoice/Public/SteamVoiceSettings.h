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

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "How the microphone opens when the game starts. The player can switch at runtime with Set Voice Mode."))
	ESteamVoiceMode DefaultMode = ESteamVoiceMode::PushToTalk;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ToolTip = "Mute players you have blocked on Steam as soon as they join the game."))
	bool bAutoMuteBlockedPlayers = true;

	UPROPERTY(Config, EditAnywhere, Category = "General", meta = (ClampMin = "0.02", ClampMax = "1.0", ToolTip = "Seconds between checks of who is talking. Only runs while voice is enabled. 0.1 is smooth enough for a talking indicator."))
	float TalkingPollSeconds = 0.1f;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "The Enhanced Input action that is held to talk (a digital action). The plugin binds it on the local player controller's Enhanced Input component: pressed = start talking, released = stop. Leave empty to drive Start Talking / Stop Talking yourself."))
	TSoftObjectPtr<UInputAction> PushToTalkAction;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "Optional mapping context that maps the push to talk action to a key. The plugin adds it to the local player, so you do not have to. Leave empty when your own contexts already contain the action."))
	TSoftObjectPtr<UInputMappingContext> PushToTalkContext;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "Priority of the push to talk mapping context."))
	int32 PushToTalkContextPriority = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Push To Talk", meta = (ToolTip = "Bind the push to talk action automatically once the local player controller has an Enhanced Input component (checked after every map load). Turn off to call Bind Steam Push To Talk yourself."))
	bool bAutoBindPushToTalk = true;

	UPROPERTY(Config, EditAnywhere, Category = "Volume", meta = (ToolTip = "The sound class the voice audio plays through. Needed for Set Master Volume. Route the voice output to this class in your audio setup."))
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Volume", meta = (ToolTip = "A sound mix (can be empty) the plugin uses to change the volume of the voice sound class at runtime."))
	TSoftObjectPtr<USoundMix> VoiceSoundMix;

	UPROPERTY(Config, EditAnywhere, Category = "Volume", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Master volume of the voice sound class when the game starts (1 = unchanged)."))
	float DefaultMasterVolume = 1.0f;
};
