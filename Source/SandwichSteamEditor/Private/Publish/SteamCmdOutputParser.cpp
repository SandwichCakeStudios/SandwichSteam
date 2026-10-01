// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamCmdOutputParser.h"

namespace
{
	void AddEvent(TArray<FSteamCmdEvent>& Out, ESteamCmdEventType Type, const FString& Text = FString(), int64 Number = 0, float Value = 0.f)
	{
		FSteamCmdEvent& Event = Out.AddDefaulted_GetRef();
		Event.Type = Type;
		Event.Text = Text;
		Event.Number = Number;
		Event.Value = Value;
	}

	/** Text inside the last "(...)" of the line, or the whole line. */
	FString ExtractReason(const FString& Line)
	{
		int32 Open = INDEX_NONE;
		int32 Close = INDEX_NONE;
		if (Line.FindLastChar(TEXT(')'), Close) && Line.FindLastChar(TEXT('('), Open) && Open < Close)
		{
			return Line.Mid(Open + 1, Close - Open - 1);
		}
		return Line;
	}

	/** Integer that follows Marker in Line, or INDEX_NONE. */
	int64 ParseNumberAfter(const FString& Line, const TCHAR* Marker)
	{
		const int32 At = Line.Find(Marker, ESearchCase::IgnoreCase);
		if (At == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		int32 Pos = At + FCString::Strlen(Marker);
		FString Digits;
		while (Line.IsValidIndex(Pos) && FChar::IsDigit(Line[Pos]))
		{
			Digits.AppendChar(Line[Pos++]);
		}
		return Digits.IsEmpty() ? INDEX_NONE : FCString::Atoi64(*Digits);
	}
}

void FSteamCmdOutputParser::Feed(const FString& Chunk, TArray<FSteamCmdEvent>& OutEvents)
{
	for (const TCHAR Char : Chunk)
	{
		if (Char == TEXT('\n') || Char == TEXT('\r'))
		{
			if (!Pending.IsEmpty())
			{
				ParseLine(Pending, OutEvents);
				Pending.Reset();
			}
		}
		else
		{
			Pending.AppendChar(Char);
		}
	}

	// A prompt waits for input, so its line never ends.
	if (!Pending.IsEmpty() && ParsePrompt(Pending, OutEvents))
	{
		Pending.Reset();
	}
}

void FSteamCmdOutputParser::Flush(TArray<FSteamCmdEvent>& OutEvents)
{
	if (!Pending.IsEmpty())
	{
		ParseLine(Pending, OutEvents);
		Pending.Reset();
	}
}

bool FSteamCmdOutputParser::ParsePrompt(const FString& Partial, TArray<FSteamCmdEvent>& OutEvents) const
{
	const FString Text = Partial.TrimStartAndEnd();
	if (Text.EndsWith(TEXT("Steam Guard code:"), ESearchCase::IgnoreCase))
	{
		AddEvent(OutEvents, ESteamCmdEventType::Line, Text);
		AddEvent(OutEvents, ESteamCmdEventType::GuardEmailRequested, Text);
		return true;
	}
	if (Text.EndsWith(TEXT("Two-factor code:"), ESearchCase::IgnoreCase))
	{
		AddEvent(OutEvents, ESteamCmdEventType::Line, Text);
		AddEvent(OutEvents, ESteamCmdEventType::GuardMobileRequested, Text);
		return true;
	}
	if (Text.EndsWith(TEXT("password:"), ESearchCase::IgnoreCase))
	{
		AddEvent(OutEvents, ESteamCmdEventType::Line, Text);
		AddEvent(OutEvents, ESteamCmdEventType::PasswordRequested, Text);
		return true;
	}
	return false;
}

void FSteamCmdOutputParser::ParseLine(const FString& RawLine, TArray<FSteamCmdEvent>& OutEvents) const
{
	const FString Line = RawLine.TrimStartAndEnd();
	if (Line.IsEmpty())
	{
		return;
	}
	AddEvent(OutEvents, ESteamCmdEventType::Line, Line);

	// SteamCMD echoes every +@Setting it was given (`"@ShutdownOnFailedCommand" = "1"`, `"@NoPromptForPassword" = "1"`), also on a clean run.
	// Their names contain words like "Failed"; they are never a result.
	if (Line.StartsWith(TEXT("\"@")))
	{
		return;
	}

	if (Line.StartsWith(TEXT("Logging in user"), ESearchCase::IgnoreCase))
	{
		if (Line.EndsWith(TEXT("OK")))
		{
			AddEvent(OutEvents, ESteamCmdEventType::LoginOk, Line);
		}
		else if (Line.Contains(TEXT("FAILED")) || Line.Contains(TEXT("ERROR")))
		{
			AddEvent(OutEvents, ESteamCmdEventType::LoginFailed, ExtractReason(Line));
		}
		return;
	}

	if (Line.Contains(TEXT("FAILED login"), ESearchCase::IgnoreCase))
	{
		AddEvent(OutEvents, ESteamCmdEventType::LoginFailed, ExtractReason(Line));
		return;
	}

	// Set by the runner (+@NoPromptForPassword 1): SteamCMD refuses to ask instead of prompting.
	if (Line.Contains(TEXT("NoPromptForPassword"), ESearchCase::IgnoreCase))
	{
		AddEvent(OutEvents, ESteamCmdEventType::PasswordRequested, Line);
		return;
	}

	if (Line.Contains(TEXT("Successfully finished"), ESearchCase::IgnoreCase))
	{
		const int64 BuildId = ParseNumberAfter(Line, TEXT("BuildID "));
		if (BuildId != INDEX_NONE)
		{
			AddEvent(OutEvents, ESteamCmdEventType::BuildFinished, Line, BuildId);
		}
		return;
	}

	if (Line.StartsWith(TEXT("ERROR"), ESearchCase::CaseSensitive) || Line.Contains(TEXT("ERROR!")) || Line.Contains(TEXT("Failed")) || Line.Contains(TEXT("FAILED")))
	{
		AddEvent(OutEvents, ESteamCmdEventType::Error, Line);
		return;
	}

	if (Line.Contains(TEXT("Checking for available updates")) || Line.Contains(TEXT("Downloading update")) || Line.Contains(TEXT("Extracting package"))
		|| Line.Contains(TEXT("Installing update")) || Line.Contains(TEXT("Verifying installation")) || Line.Contains(TEXT("Update complete, launching")))
	{
		AddEvent(OutEvents, ESteamCmdEventType::SelfUpdate, Line);
		return;
	}

	const int32 ProgressAt = Line.Find(TEXT("progress: "), ESearchCase::IgnoreCase);
	if (ProgressAt != INDEX_NONE)
	{
		const float Percent = FCString::Atof(*Line.Mid(ProgressAt + 10));
		AddEvent(OutEvents, ESteamCmdEventType::Progress, Line, 0, Percent);
		return;
	}

	// run_app_build prints "...... 55.4MB (11%)" while it scans and uploads.
	int32 Open = INDEX_NONE;
	if (Line.EndsWith(TEXT("%)")) && Line.FindLastChar(TEXT('('), Open))
	{
		AddEvent(OutEvents, ESteamCmdEventType::Progress, Line, 0, FCString::Atof(*Line.Mid(Open + 1)));
	}
}

bool FSteamCmdCommandLine::IsValidUsername(const FString& Username)
{
	if (Username.IsEmpty() || Username.Len() > 64)
	{
		return false;
	}
	for (const TCHAR Char : Username)
	{
		if (!FChar::IsAlnum(Char) && Char != TEXT('_') && Char != TEXT('.') && Char != TEXT('-') && Char != TEXT('@'))
		{
			return false;
		}
	}
	return true;
}

FString FSteamCmdCommandLine::BuildLoginCheck(const FString& Username)
{
	return FString::Printf(TEXT("+@ShutdownOnFailedCommand 1 +@NoPromptForPassword 1 +login %s +quit"), *Username);
}

FString FSteamCmdCommandLine::BuildUpload(const FString& Username, const FString& AppVdfPath)
{
	return FString::Printf(TEXT("+@ShutdownOnFailedCommand 1 +@NoPromptForPassword 1 +login %s +run_app_build \"%s\" +quit"), *Username, *AppVdfPath);
}

FString FSteamCmdCommandLine::BuildAppInfoPrint(const FString& Username, int32 AppId)
{
	// app_info_update 1 forces a fresh fetch; without it SteamCMD may print a stale or empty cached entry.
	return FString::Printf(TEXT("+@ShutdownOnFailedCommand 1 +@NoPromptForPassword 1 +login %s +app_info_update 1 +app_info_print %d +quit"), *Username, AppId);
}

FString FSteamCmdCommandLine::BuildTerminalLogin(const FString& Username)
{
	return FString::Printf(TEXT("+login %s"), *Username);
}
