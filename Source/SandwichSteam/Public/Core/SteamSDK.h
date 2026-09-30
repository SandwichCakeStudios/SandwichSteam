// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The single place that includes the Steamworks SDK headers (private to this module).
 * Always guard usage with SANDWICHSTEAM_WITH_STEAMWORKS and SandwichSteam::IsSteamClientReady()/IsSteamGameServerReady().
 * The SDK comes from the engine's Steamworks module; OnlineSubsystemSteam initializes it.
 */
#if SANDWICHSTEAM_WITH_STEAMWORKS
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
THIRD_PARTY_INCLUDES_END
#endif
