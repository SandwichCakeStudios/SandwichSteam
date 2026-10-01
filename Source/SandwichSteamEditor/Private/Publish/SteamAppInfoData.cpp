// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamAppInfoData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#define LOCTEXT_NAMESPACE "SandwichSteamAppInfoData"

namespace
{
	enum class ETokenType : uint8
	{
		String,
		Open,
		Close,
		End,
		Invalid
	};

	/** Reads the next token of KeyValues text: a quoted string, a brace or the end. */
	ETokenType ReadToken(const FString& Text, int32& Pos, FString& OutString)
	{
		const int32 Len = Text.Len();
		while (Pos < Len)
		{
			const TCHAR Char = Text[Pos];
			if (FChar::IsWhitespace(Char))
			{
				++Pos;
			}
			else if (Char == TEXT('/') && Pos + 1 < Len && Text[Pos + 1] == TEXT('/'))
			{
				while (Pos < Len && Text[Pos] != TEXT('\n'))
				{
					++Pos;
				}
			}
			else
			{
				break;
			}
		}
		if (Pos >= Len)
		{
			return ETokenType::End;
		}

		const TCHAR Char = Text[Pos];
		if (Char == TEXT('{'))
		{
			++Pos;
			return ETokenType::Open;
		}
		if (Char == TEXT('}'))
		{
			++Pos;
			return ETokenType::Close;
		}
		if (Char != TEXT('"'))
		{
			return ETokenType::Invalid;
		}

		++Pos;
		OutString.Reset();
		while (Pos < Len)
		{
			TCHAR Current = Text[Pos++];
			if (Current == TEXT('"'))
			{
				return ETokenType::String;
			}
			if (Current == TEXT('\\') && Pos < Len)
			{
				const TCHAR Escaped = Text[Pos++];
				Current = Escaped == TEXT('n') ? TEXT('\n') : Escaped == TEXT('t') ? TEXT('\t') : Escaped;
			}
			OutString.AppendChar(Current);
		}
		return ETokenType::Invalid;
	}

	/** Reads "key value" and "key { ... }" pairs into Object until the closing brace (or the end for the top level). */
	bool ParseObject(const FString& Text, int32& Pos, FJsonObject& Object, bool bTopLevel, int32 Depth, FString& OutError)
	{
		if (Depth > 32)
		{
			OutError = TEXT("Nested too deep.");
			return false;
		}

		for (;;)
		{
			FString Key;
			const ETokenType KeyToken = ReadToken(Text, Pos, Key);
			if (KeyToken == ETokenType::End)
			{
				if (bTopLevel)
				{
					return true;
				}
				OutError = TEXT("Unexpected end: a block was never closed.");
				return false;
			}
			if (KeyToken == ETokenType::Close)
			{
				if (!bTopLevel)
				{
					return true;
				}
				OutError = TEXT("Unexpected closing brace.");
				return false;
			}
			if (KeyToken != ETokenType::String)
			{
				OutError = FString::Printf(TEXT("Unexpected text at position %d."), Pos);
				return false;
			}

			FString Value;
			const ETokenType ValueToken = ReadToken(Text, Pos, Value);
			if (ValueToken == ETokenType::String)
			{
				Object.SetStringField(Key, Value);
			}
			else if (ValueToken == ETokenType::Open)
			{
				const TSharedRef<FJsonObject> Child = MakeShared<FJsonObject>();
				if (!ParseObject(Text, Pos, *Child, false, Depth + 1, OutError))
				{
					return false;
				}
				Object.SetObjectField(Key, Child);
			}
			else
			{
				OutError = FString::Printf(TEXT("Key \"%s\" has no value."), *Key);
				return false;
			}
		}
	}

	bool IsAllDigits(const FString& Text)
	{
		if (Text.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Char : Text)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		return true;
	}

	ESteamPublishPlatform PlatformFromOsList(const FString& OsList)
	{
		if (OsList.Contains(TEXT("macos"), ESearchCase::IgnoreCase) || OsList.Contains(TEXT("osx"), ESearchCase::IgnoreCase))
		{
			return ESteamPublishPlatform::Mac;
		}
		if (OsList.Contains(TEXT("linux"), ESearchCase::IgnoreCase))
		{
			return ESteamPublishPlatform::Linux;
		}
		return ESteamPublishPlatform::Win64;
	}
}

bool FSteamAppInfo::ParseKeyValues(const FString& Text, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
{
	OutRoot = MakeShared<FJsonObject>();
	int32 Pos = 0;
	if (!ParseObject(Text, Pos, *OutRoot, /*bTopLevel*/ true, 0, OutError))
	{
		OutRoot.Reset();
		return false;
	}
	return true;
}

bool FSteamAppInfo::Extract(const TSharedPtr<FJsonObject>& Root, int32 AppId, FSteamAppInfoData& OutData, FString& OutError)
{
	OutData = FSteamAppInfoData();

	const TSharedPtr<FJsonObject>* App = nullptr;
	if (!Root.IsValid() || !Root->TryGetObjectField(FString::FromInt(AppId), App))
	{
		OutError = FString::Printf(TEXT("The app info has no entry for app %d."), AppId);
		return false;
	}
	const TSharedPtr<FJsonObject>* Depots = nullptr;
	if (!(*App)->TryGetObjectField(TEXT("depots"), Depots))
	{
		OutError = TEXT("The app info has no \"depots\" section. Has a depot been created on the partner site?");
		return false;
	}

	// Depot IDs are the numeric keys. Everything else in the section ("branches", "baselanguages", ...) is not a depot.
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Depots)->Values)
	{
		const TSharedPtr<FJsonObject>* Depot = nullptr;
		if (!IsAllDigits(Pair.Key) || !Pair.Value.IsValid() || !Pair.Value->TryGetObject(Depot))
		{
			continue;
		}

		FSteamAppInfoDepot Entry;
		Entry.DepotId = FCString::Atoi(*Pair.Key);
		const TSharedPtr<FJsonObject>* Config = nullptr;
		FString OsList;
		if ((*Depot)->TryGetObjectField(TEXT("config"), Config) && (*Config)->TryGetStringField(TEXT("oslist"), OsList))
		{
			Entry.Platform = PlatformFromOsList(OsList);
		}
		OutData.Depots.Add(Entry);
	}
	OutData.Depots.Sort([](const FSteamAppInfoDepot& A, const FSteamAppInfoDepot& B) { return A.DepotId < B.DepotId; });

	const TSharedPtr<FJsonObject>* Branches = nullptr;
	if ((*Depots)->TryGetObjectField(TEXT("branches"), Branches))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Branches)->Values)
		{
			// Steam calls the public branch "public" in the app info and "default" in build scripts.
			OutData.Branches.Add(Pair.Key.Equals(TEXT("public"), ESearchCase::IgnoreCase) ? FString(TEXT("default")) : Pair.Key);
		}
		OutData.Branches.Sort([](const FString& A, const FString& B)
		{
			// default first, the rest alphabetical.
			const bool bADefault = A.Equals(TEXT("default"), ESearchCase::IgnoreCase);
			const bool bBDefault = B.Equals(TEXT("default"), ESearchCase::IgnoreCase);
			return bADefault != bBDefault ? bADefault : A < B;
		});
	}
	return true;
}

FSteamAppInfoMerge FSteamAppInfo::Preview(const USteamPublishSettings& Settings, const FSteamAppInfoData& Data)
{
	FSteamAppInfoMerge Merge;
	for (const FSteamAppInfoDepot& Depot : Data.Depots)
	{
		if (!Settings.Depots.ContainsByPredicate([&Depot](const FSteamPublishDepot& Existing) { return Existing.DepotId == Depot.DepotId; }))
		{
			Merge.NewDepotIds.Add(Depot.DepotId);
		}
	}
	for (const FString& Branch : Data.Branches)
	{
		if (!Settings.Branches.ContainsByPredicate([&Branch](const FSteamPublishBranch& Existing) { return Existing.Name.Equals(Branch, ESearchCase::IgnoreCase); }))
		{
			Merge.NewBranches.Add(Branch);
		}
	}
	return Merge;
}

void FSteamAppInfo::Apply(USteamPublishSettings& Settings, const FSteamAppInfoData& Data)
{
	const FSteamAppInfoMerge Merge = Preview(Settings, Data);
	for (const FSteamAppInfoDepot& Depot : Data.Depots)
	{
		if (Merge.NewDepotIds.Contains(Depot.DepotId))
		{
			FSteamPublishDepot& New = Settings.Depots.AddDefaulted_GetRef();
			New.DepotId = Depot.DepotId;
			New.Platform = Depot.Platform;
		}
	}
	for (const FString& Branch : Merge.NewBranches)
	{
		FSteamPublishBranch& New = Settings.Branches.AddDefaulted_GetRef();
		New.Name = Branch;
	}
}

#undef LOCTEXT_NAMESPACE
