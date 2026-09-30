// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SteamIniWriter.h"

namespace
{
	struct FIniLocation
	{
		bool bSectionFound = false;
		/** Last non-blank line of the section (its header when the section is empty). */
		int32 SectionLastLine = INDEX_NONE;
		/** Last line assigning the key (later assignments win in the engine), or INDEX_NONE. */
		int32 KeyLine = INDEX_NONE;
	};

	bool IsSectionHeader(const FString& Line, FString& OutName)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.Len() >= 2 && Trimmed[0] == TEXT('[') && Trimmed[Trimmed.Len() - 1] == TEXT(']'))
		{
			OutName = Trimmed.Mid(1, Trimmed.Len() - 2);
			return true;
		}
		return false;
	}

	/**
	 * Only the first occurrence of a duplicated section is considered.
	 * For an array element key ('+' prefix) only a line with the same value counts as the key's line.
	 */
	FIniLocation Locate(const TArray<FString>& Lines, const FString& Section, const FString& Key, FString* OutValue = nullptr, const FString* ElementValue = nullptr)
	{
		FIniLocation Location;
		bool bInSection = false;

		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			FString HeaderName;
			if (IsSectionHeader(Lines[Index], HeaderName))
			{
				if (bInSection)
				{
					break;
				}
				if (!Location.bSectionFound && HeaderName.Equals(Section, ESearchCase::IgnoreCase))
				{
					bInSection = true;
					Location.bSectionFound = true;
					Location.SectionLastLine = Index;
				}
				continue;
			}

			if (!bInSection)
			{
				continue;
			}

			const FString Trimmed = Lines[Index].TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}
			Location.SectionLastLine = Index;

			FString LineKey, LineValue;
			if (!Trimmed.StartsWith(TEXT(";")) && Trimmed.Split(TEXT("="), &LineKey, &LineValue) && LineKey.TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase)
				&& (!ElementValue || LineValue.TrimStartAndEnd().Equals(*ElementValue, ESearchCase::IgnoreCase)))
			{
				Location.KeyLine = Index;
				if (OutValue)
				{
					*OutValue = LineValue.TrimStartAndEnd();
				}
			}
		}

		return Location;
	}

	/** Splits into content lines: no line ending characters, no trailing empty element from a final newline. */
	void SplitLines(const FString& Ini, TArray<FString>& OutLines, bool& bOutCrLf)
	{
		bOutCrLf = Ini.Contains(TEXT("\r\n"));

		Ini.ParseIntoArray(OutLines, TEXT("\n"), /*bCullEmpty*/ false);
		for (FString& Line : OutLines)
		{
			if (Line.EndsWith(TEXT("\r")))
			{
				Line.LeftChopInline(1);
			}
		}

		if (OutLines.Num() > 0 && OutLines.Last().IsEmpty())
		{
			OutLines.Pop();
		}
	}
}

TArray<FSteamIniEntry> FSteamIniWriter::BuildGameEntries(bool bWithVoice)
{
	TArray<FSteamIniEntry> Entries;
	if (bWithVoice)
	{
		Entries.Add({ TEXT("/Script/Engine.GameSession"), TEXT("bRequiresPushToTalk"), TEXT("true") });
	}
	return Entries;
}

TArray<FSteamIniEntry> FSteamIniWriter::BuildRequiredEntries(int32 SteamAppId, bool bWithSessions, bool bWithVoice)
{
	TArray<FSteamIniEntry> Entries = {
		{ TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), TEXT("Steam") },
		{ TEXT("OnlineSubsystemSteam"), TEXT("bEnabled"), TEXT("true") },
		{ TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), FString::FromInt(SteamAppId) },
		// Relaunching through Steam would restart the editor/standalone process; packaged builds turn it on themselves.
		{ TEXT("OnlineSubsystemSteam"), TEXT("bRelaunchInSteam"), TEXT("false") }
	};

	if (bWithVoice)
	{
		// Without it the Steam Online Subsystem creates no voice interface.
		Entries.Add({ TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), TEXT("true") });
		// The engine's voice module creates no capture device (and so no voice interface) unless this is set.
		Entries.Add({ TEXT("Voice"), TEXT("bEnabled"), TEXT("true") });
	}

	if (bWithSessions)
	{
		// Required for a client to create/join sessions (Valve's Online Subsystem Steam docs: without it Create Session
		// does not work). The engine's SteamSockets plugin carries game traffic over Steam. The base ini already defines
		// GameNetDriver with the IP driver and the first definition wins, so the array is cleared and rebuilt (the demo
		// driver is kept for replays).
		Entries.Append({
			{ TEXT("OnlineSubsystemSteam"), TEXT("bInitServerOnClient"), TEXT("true") },
			{ TEXT("OnlineSubsystemSteam"), TEXT("bUseSteamNetworking"), TEXT("true") },
			{ TEXT("/Script/Engine.Engine"), TEXT("!NetDriverDefinitions"), TEXT("ClearArray") },
			{ TEXT("/Script/Engine.Engine"), TEXT("+NetDriverDefinitions"),
				TEXT("(DefName=\"GameNetDriver\",DriverClassName=\"/Script/SteamSockets.SteamSocketsNetDriver\",DriverClassNameFallback=\"/Script/OnlineSubsystemUtils.IpNetDriver\")") },
			{ TEXT("/Script/Engine.Engine"), TEXT("+NetDriverDefinitions"),
				TEXT("(DefName=\"DemoNetDriver\",DriverClassName=\"/Script/Engine.DemoNetDriver\",DriverClassNameFallback=\"/Script/Engine.DemoNetDriver\")") },
			{ TEXT("/Script/SteamSockets.SteamSocketsNetDriver"), TEXT("NetConnectionClassName"), TEXT("\"/Script/SteamSockets.SteamSocketsNetConnection\"") }
		});
	}

	return Entries;
}

TArray<FSteamIniChange> FSteamIniWriter::Diff(const FString& Ini, const TArray<FSteamIniEntry>& Entries)
{
	TArray<FString> Lines;
	bool bCrLf = false;
	SplitLines(Ini, Lines, bCrLf);

	TArray<FSteamIniChange> Changes;
	for (const FSteamIniEntry& Entry : Entries)
	{
		FSteamIniChange& Change = Changes.AddDefaulted_GetRef();
		Change.Entry = Entry;

		FString ExistingValue;
		const FIniLocation Location = Locate(Lines, Entry.Section, Entry.Key, &ExistingValue, Entry.Key.StartsWith(TEXT("+")) ? &Entry.Value : nullptr);
		if (Location.KeyLine == INDEX_NONE)
		{
			Change.Change = ESteamIniChange::Added;
		}
		else if (!ExistingValue.Equals(Entry.Value, ESearchCase::IgnoreCase))
		{
			Change.Change = ESteamIniChange::Modified;
			Change.OldValue = ExistingValue;
		}
	}
	return Changes;
}

FString FSteamIniWriter::Apply(const FString& Ini, const TArray<FSteamIniEntry>& Entries)
{
	TArray<FString> Lines;
	bool bCrLf = false;
	SplitLines(Ini, Lines, bCrLf);

	bool bChanged = false;
	for (const FSteamIniEntry& Entry : Entries)
	{
		FString ExistingValue;
		const FIniLocation Location = Locate(Lines, Entry.Section, Entry.Key, &ExistingValue, Entry.Key.StartsWith(TEXT("+")) ? &Entry.Value : nullptr);
		const FString NewLine = Entry.Key + TEXT("=") + Entry.Value;

		if (Location.KeyLine != INDEX_NONE)
		{
			if (!ExistingValue.Equals(Entry.Value, ESearchCase::IgnoreCase))
			{
				Lines[Location.KeyLine] = NewLine;
				bChanged = true;
			}
		}
		else if (Location.bSectionFound)
		{
			Lines.Insert(NewLine, Location.SectionLastLine + 1);
			bChanged = true;
		}
		else
		{
			if (Lines.Num() > 0 && !Lines.Last().TrimStartAndEnd().IsEmpty())
			{
				Lines.Add(FString());
			}
			Lines.Add(FString::Printf(TEXT("[%s]"), *Entry.Section));
			Lines.Add(NewLine);
			bChanged = true;
		}
	}

	if (!bChanged)
	{
		return Ini;
	}

	const FString LineEnding = bCrLf ? TEXT("\r\n") : TEXT("\n");
	return FString::Join(Lines, *LineEnding) + LineEnding;
}
