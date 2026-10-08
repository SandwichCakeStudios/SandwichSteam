// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "SteamToolSettings.generated.h"

class UTexture2D;
class USteamAppDefinition;

/** Project Settings > Plugins > Sandwich Steam */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam"))
class SANDWICHSTEAM_API USteamToolSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamToolSettings* Get();

	/** Returns true unless the feature tag is listed in DisabledFeatures. */
	bool IsFeatureEnabled(const FGameplayTag& FeatureTag) const;

	/** Applies bVerboseLogging to the LogSandwichSteam category. */
	void ApplyLogVerbosity() const;

	/** Absolute folder for files the tool writes (schema export, publish scripts, logs). Relative settings start at the project folder. */
	FString GetDataDirectory() const;

	/** Loads the assigned App Definition (small asset, loaded synchronously). Nullptr when none is assigned. */
	USteamAppDefinition* LoadAppDefinition() const;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnAppDefinitionChanged);

	/** Broadcast in the editor after the AppDefinition property was changed in the settings UI. */
	static FOnAppDefinitionChanged& OnAppDefinitionChanged();
#endif

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UDeveloperSettings

	/** Steam App ID of this game. The editor writes it into the Online Subsystem's SteamDevAppId, so this is the single source. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam", meta = (ClampMin = "0", UIMin = "1", ToolTip = "Steam App ID of this game. 480 is Valve's Spacewar test app. Use 'Configure Steam' to write it into DefaultEngine.ini."))
	int32 SteamAppId = 480;

	/** Where the tool writes its files: the schema export (Steam.Stats.ExportSchema), read again by Import from Steam, and the publish scripts. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam", meta = (RelativeToGameDir, ToolTip = "Folder for everything the tool writes: the schema export that Import from Steam reads, SteamCMD, the publish scripts, logs and history, and the staged builds. Relative paths start at the project folder. Empty = Saved/SandwichSteam."))
	FDirectoryPath DataDirectory;

	/**
	 * Configure Steam writes [OnlineSubsystemSteam] bRelaunchInSteam=false and the validator checks it. Off = the plugin neither
	 * writes nor checks the key, and the project's own value is used.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Steam", meta = (DisplayName = "Configure Steam Writes bRelaunchInSteam=false", ToolTip = "When on, Configure Steam writes bRelaunchInSteam=false into DefaultEngine.ini and the status panel checks it. With true, a Standalone run asks Steam to relaunch the game and falls back to the NULL Online Subsystem; a Shipping build is not affected either way. Turn off to manage the key yourself."))
	bool bWriteRelaunchInSteamOff = true;

	/**
	 * Networking tuning (Dashboard > Advanced). Off = the plugin neither writes nor checks any of these keys, and the engine
	 * defaults or the project's own values are used. The values only raise the engine's bandwidth caps a bit; tune them for your game.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Networking", meta = (DisplayName = "Configure Steam Writes Network Tuning", ToolTip = "When on, Configure Steam writes the bandwidth and connect timeout keys below into DefaultEngine.ini and DefaultGame.ini and the status panel checks them. Off = the engine defaults or your own values are used."))
	bool bWriteNetworkTuning = false;

	/** Per-client cap in bytes per second: MaxClientRate, MaxInternetClientRate, ConfiguredInternetSpeed, ConfiguredLanSpeed and MaxDynamicBandwidth. */
	UPROPERTY(Config, EditAnywhere, Category = "Networking", meta = (EditCondition = "bWriteNetworkTuning", ClampMin = "10000", UIMin = "10000", UIMax = "1000000", ForceUnits = "B/s", ToolTip = "Per-client bandwidth cap in bytes per second. Written to MaxClientRate, MaxInternetClientRate, ConfiguredInternetSpeed, ConfiguredLanSpeed and MaxDynamicBandwidth so the client and the server limits match."))
	int32 NetBandwidthPerClient = 200000;

	/** TotalNetBandwidth: shared by all connections. Raw value, set it for the player count of your game. */
	UPROPERTY(Config, EditAnywhere, Category = "Networking", meta = (EditCondition = "bWriteNetworkTuning", ClampMin = "10000", UIMin = "10000", UIMax = "10000000", ForceUnits = "B/s", ToolTip = "TotalNetBandwidth in bytes per second, shared by all connections. Written as is: set it for the player count of your game."))
	int32 TotalNetBandwidth = 800000;

	/** MinDynamicBandwidth: the per-connection floor when the total bandwidth is shared. */
	UPROPERTY(Config, EditAnywhere, Category = "Networking", meta = (EditCondition = "bWriteNetworkTuning", ClampMin = "1000", UIMin = "1000", UIMax = "200000", ForceUnits = "B/s", ToolTip = "MinDynamicBandwidth in bytes per second: the least a connection gets when the total bandwidth is shared. Keep it at or below the per-client cap."))
	int32 MinDynamicBandwidth = 20000;

	/** InitialConnectTimeout of the IP net driver (used by SteamSockets as well). */
	UPROPERTY(Config, EditAnywhere, Category = "Networking", meta = (EditCondition = "bWriteNetworkTuning", ClampMin = "5.0", UIMin = "5.0", UIMax = "300.0", Units = "s", ToolTip = "InitialConnectTimeout in seconds: how long a connection may take to finish the handshake. Lower fails faster, higher forgives slow Steam relay connections."))
	float InitialConnectTimeout = 60.0f;

	/** Features listed here are not created at runtime. Empty means every feature is enabled. */
	UPROPERTY(Config, EditAnywhere, Category = "Features", meta = (Categories = "Steam.Feature", ToolTip = "Features listed here are turned off and cost nothing at runtime. Leave empty to enable all features."))
	FGameplayTagContainer DisabledFeatures;

	/** Describes the stats, achievements and leaderboards of the game. Read by the Stats, Achievements and Leaderboards modules. */
	UPROPERTY(Config, EditAnywhere, Category = "Steam", meta = (AllowedClasses = "/Script/SandwichSteam.SteamAppDefinition", ToolTip = "Data asset that lists the stats, achievements and leaderboards of the game. Used by the Stats, Achievements and Leaderboards modules. The editor makes sure it is cooked."))
	TSoftObjectPtr<USteamAppDefinition> AppDefinition;

	/** Maximum number of avatar textures kept in memory (least recently used are dropped). */
	UPROPERTY(Config, EditAnywhere, Category = "User", meta = (ClampMin = "1", ClampMax = "1024", ToolTip = "Maximum number of Steam avatar textures kept in memory. The least recently used are dropped first."))
	int32 MaxCachedAvatars = 64;

	/** Texture returned for users without an avatar. Loaded lazily, only when such a user is requested. */
	UPROPERTY(Config, EditAnywhere, Category = "User", meta = (ToolTip = "Texture used for Steam users that have no avatar. Loaded only when needed. If empty, requesting such an avatar fails."))
	TSoftObjectPtr<UTexture2D> DefaultAvatar;

	/** Pause the game while the Steam overlay is open (standalone, or listen server without connected clients). */
	UPROPERTY(Config, EditAnywhere, Category = "Overlay", meta = (ToolTip = "Pause the game while the Steam overlay is open. Only pauses in standalone games or on a listen server without connected clients, never while other players are connected."))
	bool bAutoPauseOnOverlay = false;

	/** Set the game's culture from the language chosen in Steam when the Utility feature starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Utility", meta = (ToolTip = "When Steam is ready, switch the game's culture to the language selected in Steam (game language, falling back to the Steam UI language)."))
	bool bApplySteamLanguageOnStart = false;

	/** Log every Steam request and callback at Verbose level. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ToolTip = "Sets the LogSandwichSteam category to Verbose to log every Steam request and callback."))
	bool bVerboseLogging = false;
};
