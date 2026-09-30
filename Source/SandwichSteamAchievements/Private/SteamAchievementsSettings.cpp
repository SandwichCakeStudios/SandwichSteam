// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAchievementsSettings.h"

const USteamAchievementsSettings* USteamAchievementsSettings::Get()
{
	return GetDefault<USteamAchievementsSettings>();
}
