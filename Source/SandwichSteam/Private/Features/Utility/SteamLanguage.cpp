// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Utility/SteamLanguage.h"

namespace
{
	struct FLanguageEntry
	{
		const TCHAR* Steam;
		const TCHAR* Culture;
	};

	// Steam API language codes: https://partner.steamgames.com/doc/store/localization/languages
	const FLanguageEntry LanguageTable[] =
	{
		{ TEXT("arabic"), TEXT("ar") },
		{ TEXT("bulgarian"), TEXT("bg") },
		{ TEXT("schinese"), TEXT("zh-Hans") },
		{ TEXT("tchinese"), TEXT("zh-Hant") },
		{ TEXT("czech"), TEXT("cs") },
		{ TEXT("danish"), TEXT("da") },
		{ TEXT("dutch"), TEXT("nl") },
		{ TEXT("english"), TEXT("en") },
		{ TEXT("finnish"), TEXT("fi") },
		{ TEXT("french"), TEXT("fr") },
		{ TEXT("german"), TEXT("de") },
		{ TEXT("greek"), TEXT("el") },
		{ TEXT("hungarian"), TEXT("hu") },
		{ TEXT("indonesian"), TEXT("id") },
		{ TEXT("italian"), TEXT("it") },
		{ TEXT("japanese"), TEXT("ja") },
		{ TEXT("koreana"), TEXT("ko") },
		{ TEXT("norwegian"), TEXT("no") },
		{ TEXT("polish"), TEXT("pl") },
		{ TEXT("portuguese"), TEXT("pt") },
		{ TEXT("brazilian"), TEXT("pt-BR") },
		{ TEXT("romanian"), TEXT("ro") },
		{ TEXT("russian"), TEXT("ru") },
		{ TEXT("latam"), TEXT("es-419") },
		{ TEXT("spanish"), TEXT("es") },
		{ TEXT("swedish"), TEXT("sv") },
		{ TEXT("thai"), TEXT("th") },
		{ TEXT("turkish"), TEXT("tr") },
		{ TEXT("ukrainian"), TEXT("uk") },
		{ TEXT("vietnamese"), TEXT("vi") },
	};
}

namespace SandwichSteam
{
	bool SteamLanguageToCulture(const FString& SteamLanguage, FString& OutCulture)
	{
		for (const FLanguageEntry& Entry : LanguageTable)
		{
			if (SteamLanguage.Equals(Entry.Steam, ESearchCase::IgnoreCase))
			{
				OutCulture = Entry.Culture;
				return true;
			}
		}

		OutCulture.Reset();
		return false;
	}

	TArray<FString> GetSteamLanguageNames()
	{
		TArray<FString> Names;
		Names.Reserve(UE_ARRAY_COUNT(LanguageTable));
		for (const FLanguageEntry& Entry : LanguageTable)
		{
			Names.Add(Entry.Steam);
		}
		return Names;
	}
}
