// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** The tokens of one Steam language: token (#Status_InMatch) and its text with %key% substitutions. */
struct FSteamPresenceLanguage
{
	/** Steam API language name ("english", "german", "schinese", ...). */
	FString Language;

	TArray<TPair<FString, FString>> Tokens;
};

/**
 * Pure text generation of the rich presence localization file that is uploaded on the Steamworks partner site
 * (App Admin > Community > Rich Presence). No file access and no engine state, so it is unit tested with golden strings.
 * Output uses tabs and LF line endings.
 */
class FSteamPresenceVdfWriter
{
public:
	/**
	 * Turns an FText style pattern into Steam's: {map} becomes %map%. A backtick escapes the next character (`{ is a
	 * literal brace), like in FText format patterns. Keys that are not in KnownKeys are still converted and reported in
	 * OutUnknownKeys (the text would show %key% literally in Steam when the game never sets it).
	 */
	static FString ConvertPlaceholders(const FString& Pattern, const TSet<FString>& KnownKeys, TArray<FString>* OutUnknownKeys = nullptr);

	/** The file for the given languages (in the given order), skipping languages without tokens. Empty when there is nothing to write. */
	static FString Build(const TArray<FSteamPresenceLanguage>& Languages);
};
