// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamSessionsSettings.h"

const USteamSessionsSettings* USteamSessionsSettings::Get()
{
	return GetDefault<USteamSessionsSettings>();
}
