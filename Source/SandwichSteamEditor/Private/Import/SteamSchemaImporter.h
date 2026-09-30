// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USteamAppDefinition;

namespace SandwichSteam::Editor
{
	/** Path of the export the importer reads: Saved/SandwichSteam/Schema_<AppId>.json. */
	FString GetSchemaExportPath(int32 SteamAppId);

	/**
	 * "Import from Steam": merges the achievements of the schema export into Definition (asks before generating tags,
	 * never deletes rows, undoable). When Definition is null the definition assigned in the settings is used.
	 */
	void ImportFromSteam(USteamAppDefinition* Definition);
}
