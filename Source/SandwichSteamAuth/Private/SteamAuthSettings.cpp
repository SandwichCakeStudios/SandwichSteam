// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAuthSettings.h"

const USteamAuthSettings* USteamAuthSettings::Get()
{
	return GetDefault<USteamAuthSettings>();
}
