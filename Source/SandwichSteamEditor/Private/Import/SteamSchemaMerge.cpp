// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Import/SteamSchemaMerge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FSteamSchemaMerge::ParseSchema(const FString& Json, FSteamSchemaData& OutSchema, FString& OutError)
{
	OutSchema = FSteamSchemaData();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("The file is not valid JSON.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Achievements = nullptr;
	if (!Root->TryGetArrayField(TEXT("achievements"), Achievements))
	{
		OutError = TEXT("The file has no \"achievements\" array. Was it written by Steam.Stats.ExportSchema?");
		return false;
	}

	double AppId = 0.0;
	Root->TryGetNumberField(TEXT("appId"), AppId);
	OutSchema.AppId = static_cast<int32>(AppId);

	TSet<FName> Seen;
	for (const TSharedPtr<FJsonValue>& Value : *Achievements)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Row) || !Row || !Row->IsValid())
		{
			continue;
		}

		FString ApiName;
		if (!(*Row)->TryGetStringField(TEXT("apiName"), ApiName) || ApiName.IsEmpty())
		{
			continue;
		}

		// A duplicate in the export keeps the first row.
		bool bAlreadySeen = false;
		Seen.Add(FName(*ApiName), &bAlreadySeen);
		if (bAlreadySeen)
		{
			continue;
		}

		FSteamSchemaAchievement Achievement;
		Achievement.ApiName = FName(*ApiName);
		(*Row)->TryGetStringField(TEXT("displayName"), Achievement.DisplayName);
		(*Row)->TryGetStringField(TEXT("description"), Achievement.Description);
		(*Row)->TryGetBoolField(TEXT("hidden"), Achievement.bHidden);
		OutSchema.Achievements.Add(MoveTemp(Achievement));
	}

	return true;
}

FString FSteamSchemaMerge::MakeAchievementTagName(FName ApiName)
{
	if (ApiName.IsNone())
	{
		return FString();
	}

	FString Leaf = ApiName.ToString();
	for (TCHAR& Char : Leaf)
	{
		if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
		{
			Char = TEXT('_');
		}
	}
	return TEXT("Steam.Achievement.") + Leaf;
}

FSteamSchemaMergeResult FSteamSchemaMerge::MergeAchievements(TArray<FSteamAchievementDef>& Rows, const FSteamSchemaData& Schema,
	const TFunction<FGameplayTag(FName ApiName)>& MakeTag)
{
	FSteamSchemaMergeResult Result;

	TMap<FName, int32> RowByName;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		RowByName.FindOrAdd(Rows[Index].ApiName, Index);
	}

	TSet<FName> InSteam;
	for (const FSteamSchemaAchievement& Achievement : Schema.Achievements)
	{
		if (Achievement.ApiName.IsNone() || InSteam.Contains(Achievement.ApiName))
		{
			continue;
		}
		InSteam.Add(Achievement.ApiName);

		int32 RowIndex = INDEX_NONE;
		if (const int32* Found = RowByName.Find(Achievement.ApiName))
		{
			RowIndex = *Found;
		}

		bool bChanged = false;
		if (RowIndex == INDEX_NONE)
		{
			RowIndex = Rows.AddDefaulted();
			RowByName.Add(Achievement.ApiName, RowIndex);

			FSteamAchievementDef& NewRow = Rows[RowIndex];
			NewRow.ApiName = Achievement.ApiName;
			NewRow.bHidden = Achievement.bHidden;
#if WITH_EDITORONLY_DATA
			NewRow.EditorDisplayName = Achievement.DisplayName;
#endif
			Result.Added.Add(Achievement.ApiName);
		}
		else
		{
			FSteamAchievementDef& Row = Rows[RowIndex];
			if (Row.bHidden != Achievement.bHidden)
			{
				Row.bHidden = Achievement.bHidden;
				bChanged = true;
			}
#if WITH_EDITORONLY_DATA
			if (!Achievement.DisplayName.IsEmpty() && Row.EditorDisplayName != Achievement.DisplayName)
			{
				Row.EditorDisplayName = Achievement.DisplayName;
				bChanged = true;
			}
#endif
			if (bChanged)
			{
				++Result.Updated;
			}
			else
			{
				++Result.Unchanged;
			}
		}

		FSteamAchievementDef& Row = Rows[RowIndex];
		if (MakeTag && !Row.Tag.IsValid())
		{
			const FGameplayTag Tag = MakeTag(Achievement.ApiName);
			if (Tag.IsValid())
			{
				Row.Tag = Tag;
				++Result.TagsAssigned;
			}
		}
	}

	for (const FSteamAchievementDef& Row : Rows)
	{
		if (!Row.ApiName.IsNone() && !InSteam.Contains(Row.ApiName))
		{
			Result.MissingInSteam.Add(Row.ApiName);
		}
	}

	return Result;
}
