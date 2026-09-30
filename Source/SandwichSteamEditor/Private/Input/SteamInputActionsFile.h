// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USteamAppDefinition;

namespace SandwichSteam::Editor
{
	/** Path of the generated file: <Data Directory>/game_actions_<AppId>.vdf. */
	FString GetSteamInputActionsPath(int32 SteamAppId);

	/**
	 * "Generate Steam Input Actions File": writes the Steam Input action file from the Input Sets of the App Definition. Upload it on the
	 * Steamworks partner site (Steam Input > Edit Steam Input Configurations) or point the Input settings' Action Manifest at it to test.
	 * When Definition is null the definition assigned in the settings is used.
	 */
	void GenerateSteamInputActions(USteamAppDefinition* Definition);
}
