// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * One key=value line that must exist in an ini section.
 * A key that starts with '+' (an array element, for example +NetDriverDefinitions) is matched by key AND value: the line is
 * added when no line has both, and other lines with the same key are left alone.
 */
struct FSteamIniEntry
{
	FString Section;
	FString Key;
	FString Value;
};

enum class ESteamIniChange : uint8
{
	/** Key already has the wanted value. */
	Unchanged,
	/** Key (or its whole section) is missing and will be added. */
	Added,
	/** Key exists with another value and will be replaced. */
	Modified
};

struct FSteamIniChange
{
	FSteamIniEntry Entry;
	ESteamIniChange Change = ESteamIniChange::Unchanged;
	/** Previous value when Change is Modified. */
	FString OldValue;
};

/**
 * Pure ini text editing (string in, string out, no file or engine access) so it can be unit tested.
 * Idempotent: Apply(Apply(Ini, E), E) == Apply(Ini, E), and Apply returns the input untouched when nothing changes.
 * Existing content, comments, ordering and the line ending style are preserved.
 */
class FSteamIniWriter
{
public:
	/**
	 * The DefaultEngine.ini entries Sandwich Steam needs for the given Steam App ID.
	 * With bWithSessions the SteamSockets net driver entries of the Sessions module are included.
	 * With bWithVoice the Steam voice interface is switched on ([OnlineSubsystem] bHasVoiceEnabled and [Voice] bEnabled).
	 */
	static TArray<FSteamIniEntry> BuildRequiredEntries(int32 SteamAppId, bool bWithSessions = false, bool bWithVoice = false);

	/**
	 * The DefaultGame.ini entries: with bWithVoice the game session asks for push to talk ([/Script/Engine.GameSession] bRequiresPushToTalk=true).
	 * The Voice module opens the microphone itself in open mic mode, so the ini value does not depend on the mode.
	 */
	static TArray<FSteamIniEntry> BuildGameEntries(bool bWithVoice);

	/** What Apply would do, per entry. */
	static TArray<FSteamIniChange> Diff(const FString& Ini, const TArray<FSteamIniEntry>& Entries);

	/** Returns Ini with every entry set. Missing sections are appended, missing keys added at the end of their section. */
	static FString Apply(const FString& Ini, const TArray<FSteamIniEntry>& Entries);
};
