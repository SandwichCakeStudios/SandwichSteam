// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USteamAppDefinition;

namespace SandwichSteam::Editor
{
	/** Path of the generated file: <Data Directory>/richpresence_<AppId>.vdf. */
	FString GetPresenceLocalizationPath(int32 SteamAppId);

	/**
	 * "Generate Rich Presence Localization": writes the rich presence localization file (one block per Steam language,
	 * tokens and texts of the presence rows) for upload on the Steamworks partner site. When Definition is null the
	 * definition assigned in the settings is used.
	 */
	void GeneratePresenceLocalization(USteamAppDefinition* Definition);
}
