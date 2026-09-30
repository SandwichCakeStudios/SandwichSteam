// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceSettings.h"

const USteamVoiceSettings* USteamVoiceSettings::Get()
{
	return GetDefault<USteamVoiceSettings>();
}
