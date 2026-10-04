// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamBackend.h"
#include "Core/SteamSDK.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

namespace SandwichSteam
{
	IOnlineSubsystem* GetSteamOSS(const UObject* WorldContext)
	{
		// IsEnabled reads config only, so a disabled Steam subsystem does not log errors or get created.
		if (!IOnlineSubsystem::IsEnabled(STEAM_SUBSYSTEM))
		{
			return nullptr;
		}

		const UWorld* World = (WorldContext && GEngine)
			? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
			: nullptr;

		// Only the default subsystem counts (per world: PIE uses one OSS instance per world context). Asking for STEAM_SUBSYSTEM
		// by name creates a second Steam instance after the engine gave up on Steam (relaunch request, Steam not running) and
		// fell back to NULL; the game would then run on NULL while the plugin reported Steam as ready (Phase 14b).
		IOnlineSubsystem* DefaultOSS = World ? Online::GetSubsystem(World) : IOnlineSubsystem::Get();
		return DefaultOSS && DefaultOSS->GetSubsystemName() == STEAM_SUBSYSTEM ? DefaultOSS : nullptr;
	}

	bool IsSteamOSSAvailable(const UObject* WorldContext)
	{
		return GetSteamOSS(WorldContext) != nullptr;
	}

	bool IsSteamClientReady(const UObject* WorldContext)
	{
#if SANDWICHSTEAM_WITH_STEAMWORKS
		// The OSS check must come first: it guarantees the Steam library is loaded before any raw call.
		if (!IsSteamOSSAvailable(WorldContext))
		{
			return false;
		}

		ISteamUser* SteamUserPtr = SteamUser();
		return SteamUserPtr != nullptr && SteamUserPtr->BLoggedOn();
#else
		return false;
#endif
	}

	bool IsSteamGameServerReady(const UObject* WorldContext)
	{
#if SANDWICHSTEAM_WITH_STEAMWORKS
		if (!IsSteamOSSAvailable(WorldContext))
		{
			return false;
		}

		ISteamGameServer* SteamGameServerPtr = SteamGameServer();
		return SteamGameServerPtr != nullptr && SteamGameServerPtr->BLoggedOn();
#else
		return false;
#endif
	}
}
