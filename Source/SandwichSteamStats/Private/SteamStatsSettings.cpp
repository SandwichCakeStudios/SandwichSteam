// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamStatsSettings.h"

const USteamStatsSettings* USteamStatsSettings::Get()
{
	return GetDefault<USteamStatsSettings>();
}
