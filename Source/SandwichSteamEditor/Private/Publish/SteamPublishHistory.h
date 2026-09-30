// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSteamPublishHistoryEntry
{
	FDateTime Time;
	int32 AppId = 0;
	FString Branch;
	/** 0 when Steam reported none. */
	int64 BuildId = 0;
	bool bSuccess = false;
	/** Result message or first error line. */
	FString Message;
};

/**
 * Upload history, newest first, kept in <Publish Directory>/History.json. Serialization is pure and unit tested;
 * Load and Append touch the file.
 */
class FSteamPublishHistory
{
public:
	static constexpr int32 MaxEntries = 50;

	static FString ToJson(const TArray<FSteamPublishHistoryEntry>& Entries);
	static bool FromJson(const FString& Json, TArray<FSteamPublishHistoryEntry>& OutEntries);

	static FString GetFilePath();
	static TArray<FSteamPublishHistoryEntry> Load();

	/** Puts Entry first and trims to MaxEntries. */
	static void Append(const FSteamPublishHistoryEntry& Entry);
};
