// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Presence/SteamPresenceVdfWriter.h"
#include "Publish/SteamVdfWriter.h"

FString FSteamPresenceVdfWriter::ConvertPlaceholders(const FString& Pattern, const TSet<FString>& KnownKeys, TArray<FString>* OutUnknownKeys)
{
	FString Result;
	Result.Reserve(Pattern.Len() + 8);

	const int32 Length = Pattern.Len();
	for (int32 Index = 0; Index < Length; ++Index)
	{
		const TCHAR Char = Pattern[Index];

		if (Char == TEXT('`') && Index + 1 < Length)
		{
			Result.AppendChar(Pattern[++Index]); // Escaped character, taken literally.
			continue;
		}

		if (Char == TEXT('{'))
		{
			const int32 Close = Pattern.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 1);
			if (Close != INDEX_NONE)
			{
				const FString Key = Pattern.Mid(Index + 1, Close - Index - 1).TrimStartAndEnd();
				if (!Key.IsEmpty())
				{
					if (OutUnknownKeys && !KnownKeys.Contains(Key))
					{
						OutUnknownKeys->AddUnique(Key);
					}
					Result += TEXT("%") + Key + TEXT("%");
					Index = Close;
					continue;
				}
			}
		}

		Result.AppendChar(Char);
	}

	return Result;
}

FString FSteamPresenceVdfWriter::Build(const TArray<FSteamPresenceLanguage>& Languages)
{
	FString Out;
	bool bAny = false;

	for (const FSteamPresenceLanguage& Language : Languages)
	{
		if (Language.Tokens.IsEmpty())
		{
			continue;
		}

		if (!bAny)
		{
			Out += TEXT("\"lang\"\n{\n");
			bAny = true;
		}

		Out += FString::Printf(TEXT("\t%s\n\t{\n\t\t\"tokens\"\n\t\t{\n"), *FSteamVdfWriter::Quote(Language.Language));
		for (const TPair<FString, FString>& Token : Language.Tokens)
		{
			Out += FString::Printf(TEXT("\t\t\t%s\t%s\n"), *FSteamVdfWriter::Quote(Token.Key), *FSteamVdfWriter::Quote(Token.Value));
		}
		Out += TEXT("\t\t}\n\t}\n");
	}

	if (bAny)
	{
		Out += TEXT("}\n");
	}
	return Out;
}
