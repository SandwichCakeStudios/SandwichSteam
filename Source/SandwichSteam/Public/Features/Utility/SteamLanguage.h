// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SandwichSteam
{
	/**
	 * Maps a Steam API language name ("english", "schinese", "brazilian", ...) to an Unreal culture name ("en", "zh-Hans", "pt-BR", ...).
	 * Case insensitive. Returns false and an empty OutCulture for unknown languages.
	 */
	SANDWICHSTEAM_API bool SteamLanguageToCulture(const FString& SteamLanguage, FString& OutCulture);

	/** Every Steam API language name this table knows ("arabic", ..., "vietnamese"), in table order. */
	SANDWICHSTEAM_API TArray<FString> GetSteamLanguageNames();
}
