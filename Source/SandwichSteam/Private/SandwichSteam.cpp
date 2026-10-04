// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteam.h"
#include "Core/SteamBackend.h"
#include "Core/SteamEngineCompat.h"
#include "Core/SteamLog.h"
#include "Core/SteamSDK.h"
#include "Core/SteamToolSettings.h"
#include "Debug/SteamConsoleCommands.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"

void FSandwichSteamModule::StartupModule()
{
	RegisterTagIniSearchPath();

	PostEngineInitHandle = SandwichSteam::Compat::OnPostEngineInit().AddRaw(this, &FSandwichSteamModule::HandlePostEngineInit);

#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::RegisterCoreCommands();
	SandwichSteam::Debug::RegisterFeatureCommands();
#endif

	UE_LOG(LogSandwichSteam, Log, TEXT("SandwichSteam started (SANDWICHSTEAM_WITH_STEAMWORKS=%d, SANDWICHSTEAM_WITH_DEBUG=%d)"),
		SANDWICHSTEAM_WITH_STEAMWORKS, SANDWICHSTEAM_WITH_DEBUG);
}

void FSandwichSteamModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterCommands();
#endif

	if (PostEngineInitHandle.IsValid())
	{
		SandwichSteam::Compat::OnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
}

void FSandwichSteamModule::RegisterTagIniSearchPath()
{
	// Designer-extensible tags (Steam.Achievement.*, Steam.Presence.*, ...) live in Config/Tags/*.ini,
	// which the Gameplay Tags Manager does not pick up from plugins on its own.
	const TSharedPtr<IPlugin> ThisPlugin = IPluginManager::Get().FindPlugin(TEXT("SandwichSteam"));
	if (!ThisPlugin.IsValid())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("SandwichSteam plugin not found; plugin gameplay tag ini files are not registered."));
		return;
	}

	const FString TagIniPath = ThisPlugin->GetBaseDir() / TEXT("Config") / TEXT("Tags");
	UGameplayTagsManager::Get().AddTagIniSearchPath(TagIniPath);
}

void FSandwichSteamModule::HandlePostEngineInit()
{
	// Settings are fully loaded from config by now.
	if (const USteamToolSettings* Settings = USteamToolSettings::Get())
	{
		Settings->ApplyLogVerbosity();
	}

	if (!SandwichSteam::IsSteamOSSAvailable(nullptr))
	{
#if UE_BUILD_SHIPPING
		// Shipping builds write no steam_appid.txt, so started outside the Steam client they get no Steam (verified 2026-10-03,
		// Phase 14b). Only visible in projects that enable logging in Shipping.
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam Online Subsystem not available. Steam features stay inactive. Most likely this Shipping build was started outside the Steam client (double-clicking the exe): start it from Steam. To test outside Steam use a Development build. Also check that Steam is running and that OnlineSubsystemSteam is enabled with DefaultPlatformService=Steam."));
#else
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Online Subsystem not available. Steam features stay inactive. Enable OnlineSubsystemSteam and set DefaultPlatformService=Steam in DefaultEngine.ini."));
#endif
		return;
	}

	const bool bClientReady = SandwichSteam::IsSteamClientReady(nullptr);
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Online Subsystem detected (client ready: %s, game server ready: %s)."),
		bClientReady ? TEXT("yes") : TEXT("no"),
		SandwichSteam::IsSteamGameServerReady(nullptr) ? TEXT("yes") : TEXT("no"));

	if (bClientReady)
	{
		WarnOnAppIdMismatch();
	}
}

void FSandwichSteamModule::WarnOnAppIdMismatch()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	// Raw SteamUtils (as the debug dump) keeps the core free of a link dependency on OnlineSubsystemSteam.
	ISteamUtils* Utils = SteamUtils(); // Steam's getters are not const.
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	if (!Utils || !Settings || Settings->SteamAppId <= 0)
	{
		return;
	}

	const uint32 RunningAppId = Utils->GetAppID();
	if (RunningAppId != 0 && RunningAppId != static_cast<uint32>(Settings->SteamAppId))
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam is running App ID %u, but Project Settings > Plugins > Sandwich Steam says %d. Run Configure Steam (SteamDevAppId) and delete stale steam_appid.txt files, or fix the setting."),
			RunningAppId, Settings->SteamAppId);
	}
#endif
}

FSandwichSteamModule& FSandwichSteamModule::Get()
{
	return FModuleManager::LoadModuleChecked<FSandwichSteamModule>(TEXT("SandwichSteam"));
}

bool FSandwichSteamModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("SandwichSteam"));
}

IMPLEMENT_MODULE(FSandwichSteamModule, SandwichSteam)
