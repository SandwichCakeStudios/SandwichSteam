// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/SteamAppDefinition.h"
#include "GameplayTagContainer.h"

/** One achievement as exported by Steam.Stats.ExportSchema. */
struct FSteamSchemaAchievement
{
	FName ApiName;
	FString DisplayName;
	FString Description;
	bool bHidden = false;
};

/** Parsed Saved/SandwichSteam/Schema_<AppId>.json. */
struct FSteamSchemaData
{
	int32 AppId = 0;
	TArray<FSteamSchemaAchievement> Achievements;
};

/** What a merge did. Nothing is ever deleted. */
struct FSteamSchemaMergeResult
{
	/** API names of the rows that were added. */
	TArray<FName> Added;

	/** Rows whose hidden flag or editor display name changed. */
	int32 Updated = 0;

	/** Rows that already matched. */
	int32 Unchanged = 0;

	/** Rows of the definition that Steam does not report (kept, reported to the user). */
	TArray<FName> MissingInSteam;

	/** Rows that received a gameplay tag. */
	int32 TagsAssigned = 0;
};

/**
 * Pure logic of "Import from Steam" (no engine, file or UI access) so it can be unit tested.
 * Merge rules: rows are matched by ApiName; existing rows keep their tag, progress stat and progress max; only the
 * hidden flag and the editor display name follow Steam; new rows are appended in the order Steam lists them.
 */
class FSteamSchemaMerge
{
public:
	/** Parses the export. Returns false and fills OutError when the file is not a schema export. */
	static bool ParseSchema(const FString& Json, FSteamSchemaData& OutSchema, FString& OutError);

	/**
	 * Gameplay tag name generated for an achievement: Steam.Achievement.<ApiName> with every character that is not a letter,
	 * digit or underscore replaced by an underscore. Empty when the name is empty.
	 */
	static FString MakeAchievementTagName(FName ApiName);

	/**
	 * Merges the achievements of Schema into Rows.
	 * MakeTag (optional) is called for every row without a valid tag (new rows and existing ones) and returns the tag to assign,
	 * or an invalid tag to leave the row untagged.
	 */
	static FSteamSchemaMergeResult MergeAchievements(TArray<FSteamAchievementDef>& Rows, const FSteamSchemaData& Schema,
		const TFunction<FGameplayTag(FName ApiName)>& MakeTag);
};
