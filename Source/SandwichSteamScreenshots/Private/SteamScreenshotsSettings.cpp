// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamScreenshotsSettings.h"

const USteamScreenshotsSettings* USteamScreenshotsSettings::Get()
{
	return GetDefault<USteamScreenshotsSettings>();
}
