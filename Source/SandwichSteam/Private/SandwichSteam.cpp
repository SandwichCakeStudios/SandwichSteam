// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteam.h"
#include "Core/SteamBackend.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Debug/SteamConsoleCommands.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"

void FSandwichSteamModule::StartupModule()
{
	RegisterTagIniSearchPath();

	PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FSandwichSteamModule::HandlePostEngineInit);

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
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
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
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Online Subsystem not available. Steam features stay inactive. Enable OnlineSubsystemSteam and set DefaultPlatformService=Steam in DefaultEngine.ini."));
		return;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Online Subsystem detected (client ready: %s, game server ready: %s)."),
		SandwichSteam::IsSteamClientReady(nullptr) ? TEXT("yes") : TEXT("no"),
		SandwichSteam::IsSteamGameServerReady(nullptr) ? TEXT("yes") : TEXT("no"));
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
