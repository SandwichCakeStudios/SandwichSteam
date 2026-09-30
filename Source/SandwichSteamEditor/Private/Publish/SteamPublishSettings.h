// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamPublishSettings.generated.h"

/** Platform of one Steam depot. */
UENUM()
enum class ESteamPublishPlatform : uint8
{
	Win64,
	Mac,
	Linux
};

/** Build configuration the packager cooks for. */
UENUM()
enum class ESteamPublishConfig : uint8
{
	Development,
	Shipping
};

/** One FileMapping entry of a depot build script. */
USTRUCT()
struct FSteamPublishFileMapping
{
	GENERATED_BODY()

	/** Path (or wildcard) relative to the depot content root. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString LocalPath = TEXT("*");

	/** Destination inside the depot. "." is the depot root. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString DepotPath = TEXT(".");

	/** Include sub folders. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	bool bRecursive = true;
};

/** One depot of the app. One per platform is the usual setup. */
USTRUCT()
struct FSteamPublishDepot
{
	GENERATED_BODY()

	FSteamPublishDepot()
	{
		FileMappings.Add(FSteamPublishFileMapping());
		FileExclusions = { TEXT("*.pdb"), TEXT("*.debug"), TEXT("Manifest_*.txt"), TEXT("steam_appid.txt") };
	}

	/** Disabled depots are skipped by the build. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	bool bEnabled = true;

	/** Platform this depot is packaged for. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	ESteamPublishPlatform Platform = ESteamPublishPlatform::Win64;

	/** Depot ID from the Steamworks partner site (App Admin > SteamPipe > Depots). */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish", meta = (ClampMin = "0"))
	int32 DepotId = 0;

	/** Folder that holds the packaged game. Relative paths start at the project folder. Empty = <Staging Directory>/<Platform> (see Folders). */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString ContentRoot;

	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	TArray<FSteamPublishFileMapping> FileMappings;

	/** Files that are never uploaded. steam_appid.txt is always excluded, even when it is removed from this list. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	TArray<FString> FileExclusions;
};

/** A Steam branch the tool can upload to. */
USTRUCT()
struct FSteamPublishBranch
{
	GENERATED_BODY()

	/** Branch name as created on the partner site. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString Name;

	/** Set the uploaded build live on this branch. Steam does not allow this for "default"; it is ignored there. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	bool bSetLive = true;
};

/** A command that runs before packaging or after the upload. */
USTRUCT()
struct FSteamPublishStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString Executable;

	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString Arguments;

	/** Relative paths start at the project folder. Empty = project folder. */
	UPROPERTY(EditAnywhere, Config, Category = "Steam Publish")
	FString WorkingDirectory;
};

/**
 * Project Settings > Plugins > Sandwich Steam - Publish. Shared with the team (DefaultEditor.ini): depots, branches, description, packaging.
 * The App ID comes from the runtime settings (Sandwich Steam) so it exists in one place.
 * Credentials are never stored here; see USteamPublishUserSettings.
 */
UCLASS(Config = Editor, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Publish"))
class USteamPublishSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USteamPublishSettings();

	static const USteamPublishSettings* Get();

	/** App ID of the runtime settings. */
	int32 GetAppId() const;

	/** SteamCmdDownloadUrlWindows/Mac/Linux, picked by PLATFORM_WINDOWS/MAC/LINUX. Empty if none applies (an unsupported host). */
	FString GetSteamCmdDownloadUrl() const;

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings

	UPROPERTY(EditAnywhere, Config, Category = "Depots", meta = (TitleProperty = "DepotId"))
	TArray<FSteamPublishDepot> Depots;

	UPROPERTY(EditAnywhere, Config, Category = "Branches", meta = (TitleProperty = "Name"))
	TArray<FSteamPublishBranch> Branches;

	/** Build description shown in the partner site. Tokens: {Project}, {Config}, {Branch}, {Date}. */
	UPROPERTY(EditAnywhere, Config, Category = "Build")
	FString BuildDescriptionTemplate = TEXT("{Project} {Config} {Date}");

	/** Folder for the generated build scripts, SteamCMD output and build history. Relative paths start at the project folder. Empty = <Data Directory>/Publish (Sandwich Steam settings). */
	UPROPERTY(EditAnywhere, Config, Category = "Folders", meta = (RelativeToGameDir))
	FDirectoryPath PublishDirectory;

	/** Where the packager stages the game and where depots without their own content root look. One sub folder per platform (Windows, Mac, Linux). Empty = Saved/StagedBuilds. */
	UPROPERTY(EditAnywhere, Config, Category = "Folders", meta = (RelativeToGameDir))
	FDirectoryPath StagingDirectory;

	/** Configuration the packager builds. */
	UPROPERTY(EditAnywhere, Config, Category = "Build")
	ESteamPublishConfig PackageConfig = ESteamPublishConfig::Shipping;

	/**
	 * Passed to BuildCookRun as -target=<Name>. Leave empty to let UAT pick the project's only (or default) Game target.
	 * Required when the project has more than one Target.cs under Source/ for different variants - e.g. Lyra ships
	 * LyraClient, LyraGameEOS and LyraGame - otherwise BuildCookRun may build the wrong one or fail as ambiguous.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "Build")
	FString TargetName;

	/** Run before packaging. */
	UPROPERTY(EditAnywhere, Config, Category = "Build", meta = (TitleProperty = "Executable"))
	TArray<FSteamPublishStep> PreSteps;

	/** Run after a successful upload. */
	UPROPERTY(EditAnywhere, Config, Category = "Build", meta = (TitleProperty = "Executable"))
	TArray<FSteamPublishStep> PostSteps;

	/** Windows SteamCMD archive (.zip). Edit if Valve changes it; see https://developer.valvesoftware.com/wiki/SteamCMD (See SteamCMD offical documentation). */
	UPROPERTY(EditAnywhere, Config, Category = "SteamCMD Download")
	FString SteamCmdDownloadUrlWindows = TEXT("https://client-update.steamstatic.com/installer/steamcmd.zip");

	/** Mac SteamCMD archive (.tar.gz). Edit if Valve changes it; see https://developer.valvesoftware.com/wiki/SteamCMD (See SteamCMD offical documentation). */
	UPROPERTY(EditAnywhere, Config, Category = "SteamCMD Download")
	FString SteamCmdDownloadUrlMac = TEXT("https://client-update.steamstatic.com/installer/steamcmd_osx.tar.gz");

	/** Linux SteamCMD archive (.tar.gz). Edit if Valve changes it; see https://developer.valvesoftware.com/wiki/SteamCMD (See SteamCMD offical documentation). */
	UPROPERTY(EditAnywhere, Config, Category = "SteamCMD Download")
	FString SteamCmdDownloadUrlLinux = TEXT("https://client-update.steamstatic.com/installer/steamcmd_linux.tar.gz");
};

/**
 * Per developer settings (EditorPerProjectUserSettings.ini, never committed): where SteamCMD is and which Steam account uploads.
 * The password is never stored or passed to SteamCMD by this tool. Log in once in a terminal; SteamCMD caches the login.
 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Sandwich Steam - Publish (User)"))
class USteamPublishUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamPublishUserSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings

	/** steamcmd executable (steamcmd.exe on Windows). Download from developer.valvesoftware.com/wiki/SteamCMD. */
	UPROPERTY(EditAnywhere, Config, Category = "SteamCMD")
	FFilePath SteamCmdPath;

	/** Steam account that has the "Edit App Metadata" and "Publish App Changes To Steam" permissions. */
	UPROPERTY(EditAnywhere, Config, Category = "SteamCMD")
	FString SteamUsername;
};
