// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Publish/SteamPublishSettings.h"

class FJsonObject;

/** One depot found in the app info. */
struct FSteamAppInfoDepot
{
	int32 DepotId = 0;
	ESteamPublishPlatform Platform = ESteamPublishPlatform::Win64;
};

/** What the tool takes from the app info: the depots and branches defined on the partner site. */
struct FSteamAppInfoData
{
	TArray<FSteamAppInfoDepot> Depots;
	/** Names as the publish tool uses them: Steam's "public" branch is "default" in build scripts. */
	TArray<FString> Branches;
};

/** What applying the app info to the publish settings adds. Nothing is ever removed or overwritten. */
struct FSteamAppInfoMerge
{
	TArray<int32> NewDepotIds;
	TArray<FString> NewBranches;
};

/** Pure helpers (no process, no settings writes unless asked), unit testable. */
struct FSteamAppInfo
{
	/** Valve KeyValues text (what app_info_print prints) to a JSON object. A repeated key keeps the last value. */
	static bool ParseKeyValues(const FString& Text, TSharedPtr<FJsonObject>& OutRoot, FString& OutError);

	/** Reads depots and branches of the app out of the object ParseKeyValues made. */
	static bool Extract(const TSharedPtr<FJsonObject>& Root, int32 AppId, FSteamAppInfoData& OutData, FString& OutError);

	/** Depots (by ID) and branches (by name, ignoring case) that Settings does not have yet. */
	static FSteamAppInfoMerge Preview(const USteamPublishSettings& Settings, const FSteamAppInfoData& Data);

	/** Adds what Preview reports. Does not save. */
	static void Apply(USteamPublishSettings& Settings, const FSteamAppInfoData& Data);
};
