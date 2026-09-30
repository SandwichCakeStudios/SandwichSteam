// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamCloudSettings.h"

const USteamCloudSettings* USteamCloudSettings::Get()
{
	return GetDefault<USteamCloudSettings>();
}
