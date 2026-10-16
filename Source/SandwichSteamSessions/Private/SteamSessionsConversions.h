// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamSDK.h"
#include "SteamSessionTypes.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

/** Plugin enum to Steamworks enum conversions shared by the session and lobby backends. */
namespace SandwichSteam::Sessions
{
	inline ELobbyDistanceFilter ToSteamDistance(ESteamSessionDistance Distance)
	{
		switch (Distance)
		{
		case ESteamSessionDistance::Close: return k_ELobbyDistanceFilterClose;
		case ESteamSessionDistance::Far: return k_ELobbyDistanceFilterFar;
		case ESteamSessionDistance::Worldwide: return k_ELobbyDistanceFilterWorldwide;
		default: return k_ELobbyDistanceFilterDefault;
		}
	}
}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
