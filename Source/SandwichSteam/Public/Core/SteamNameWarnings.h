// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"

namespace SandwichSteam
{
	/**
	 * Used by the by-name / by-App-ID Blueprint nodes. When Result failed with Steam.Error.InvalidArgument or Steam.Error.Failed (how an
	 * unknown name or a typo in a pin shows up), logs one warning per Feature + Name; other failures (not ready, inactive, offline) are
	 * normal at startup and stay silent. Returns Result unchanged.
	 * Game thread. At most 256 names are remembered, later ones are not logged.
	 */
	SANDWICHSTEAM_API FSteamResult WarnOnceIfInvalid(const TCHAR* Feature, const FString& Name, const FSteamResult& Result);
}
