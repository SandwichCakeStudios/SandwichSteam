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

		// With a world, resolve the per-world instance (PIE uses one OSS instance per world context).
		return World ? Online::GetSubsystem(World, STEAM_SUBSYSTEM) : IOnlineSubsystem::Get(STEAM_SUBSYSTEM);
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
