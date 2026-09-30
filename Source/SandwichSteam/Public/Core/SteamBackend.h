// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IOnlineSubsystem;

/**
 * Helpers to resolve the Steam host. Steam is initialized by the engine's OnlineSubsystemSteam;
 * Sandwich Steam only observes it. Everything here is null-safe and works without Steam.
 */
namespace SandwichSteam
{
	/**
	 * Steam Online Subsystem for the world of WorldContext (correct instance in PIE), or nullptr
	 * if Steam is disabled in config or failed to start. Pass nullptr only outside of any world (module-level code).
	 */
	SANDWICHSTEAM_API IOnlineSubsystem* GetSteamOSS(const UObject* WorldContext);

	/** True when the Steam Online Subsystem exists for this world. Does not imply a logged-on client. */
	SANDWICHSTEAM_API bool IsSteamOSSAvailable(const UObject* WorldContext);

	/**
	 * True when raw Steamworks client calls are safe: SDK compiled in, Steam OSS running,
	 * client interfaces initialized and the user logged on. False on dedicated servers.
	 */
	SANDWICHSTEAM_API bool IsSteamClientReady(const UObject* WorldContext);

	/** True when raw Steamworks game server calls are safe (dedicated or listen server with Steam game server API). */
	SANDWICHSTEAM_API bool IsSteamGameServerReady(const UObject* WorldContext);
}
