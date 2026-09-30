// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamInputSettings.h"

const USteamInputSettings* USteamInputSettings::Get()
{
	return GetDefault<USteamInputSettings>();
}
