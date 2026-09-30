// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishHistory.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Publish/SteamPublishVdf.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FSteamPublishHistory::ToJson(const TArray<FSteamPublishHistoryEntry>& Entries)
{
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FSteamPublishHistoryEntry& Entry : Entries)
	{
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("time"), Entry.Time.ToIso8601());
		Row->SetNumberField(TEXT("appId"), Entry.AppId);
		Row->SetStringField(TEXT("branch"), Entry.Branch);
		// Build IDs fit a double exactly (they are far below 2^53), but keep them as text to be safe.
		Row->SetStringField(TEXT("buildId"), FString::Printf(TEXT("%lld"), Entry.BuildId));
		Row->SetBoolField(TEXT("success"), Entry.bSuccess);
		Row->SetStringField(TEXT("message"), Entry.Message);
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("entries"), Rows);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json;
}

bool FSteamPublishHistory::FromJson(const FString& Json, TArray<FSteamPublishHistoryEntry>& OutEntries)
{
	OutEntries.Reset();

	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetArrayField(TEXT("entries"), Rows))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Row))
		{
			continue;
		}

		FSteamPublishHistoryEntry Entry;
		FString Time, BuildId;
		(*Row)->TryGetStringField(TEXT("time"), Time);
		FDateTime::ParseIso8601(*Time, Entry.Time);
		(*Row)->TryGetNumberField(TEXT("appId"), Entry.AppId);
		(*Row)->TryGetStringField(TEXT("branch"), Entry.Branch);
		(*Row)->TryGetStringField(TEXT("buildId"), BuildId);
		Entry.BuildId = FCString::Atoi64(*BuildId);
		(*Row)->TryGetBoolField(TEXT("success"), Entry.bSuccess);
		(*Row)->TryGetStringField(TEXT("message"), Entry.Message);
		OutEntries.Add(MoveTemp(Entry));
	}
	return true;
}

FString FSteamPublishHistory::GetFilePath()
{
	return SandwichSteam::Publish::GetPublishDir() / TEXT("History.json");
}

TArray<FSteamPublishHistoryEntry> FSteamPublishHistory::Load()
{
	TArray<FSteamPublishHistoryEntry> Entries;
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *GetFilePath()))
	{
		FromJson(Json, Entries);
	}
	return Entries;
}

void FSteamPublishHistory::Append(const FSteamPublishHistoryEntry& Entry)
{
	TArray<FSteamPublishHistoryEntry> Entries = Load();
	Entries.Insert(Entry, 0);
	if (Entries.Num() > MaxEntries)
	{
		Entries.SetNum(MaxEntries);
	}
	FFileHelper::SaveStringToFile(ToJson(Entries), *GetFilePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
