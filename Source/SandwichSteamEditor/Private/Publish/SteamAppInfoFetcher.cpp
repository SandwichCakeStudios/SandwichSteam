// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamAppInfoFetcher.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Publish/SteamAppInfoData.h"
#include "Publish/SteamCmdRunner.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamPublishVdf.h"

#define LOCTEXT_NAMESPACE "SandwichSteamAppInfoFetcher"

FSteamAppInfoFetcher::FSteamAppInfoFetcher() = default;
FSteamAppInfoFetcher::~FSteamAppInfoFetcher() = default;

bool FSteamAppInfoFetcher::IsRunning() const
{
	return Runner.IsValid() && Runner->IsRunning();
}

FString FSteamAppInfoFetcher::GetOutputPath(int32 AppId)
{
	return SandwichSteam::Publish::GetPublishDir() / TEXT("AppInfo") / FString::Printf(TEXT("app_info_%d.json"), AppId);
}

bool FSteamAppInfoFetcher::Start(FString& OutError)
{
	check(IsInGameThread());
	if (IsRunning())
	{
		OutError = LOCTEXT("AlreadyRunning", "App info is already being fetched.").ToString();
		return false;
	}

	AppId = USteamPublishSettings::Get()->GetAppId();
	if (AppId <= 0)
	{
		OutError = LOCTEXT("NoAppId", "No App ID is set. Fill it in under Project Settings > Sandwich Steam.").ToString();
		return false;
	}

	const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
	const FString UserName = User->SteamUsername.TrimStartAndEnd();
	if (!FSteamCmdCommandLine::IsValidUsername(UserName))
	{
		OutError = LOCTEXT("BadUser", "The Steam account name is missing or invalid.").ToString();
		return false;
	}

	Lines.Reset();
	bGuardRequested = false;

	Runner = MakeShared<FSteamCmdRunner>();
	Runner->OnEvent().AddSP(this, &FSteamAppInfoFetcher::HandleEvent);
	Runner->OnFinished().AddSP(this, &FSteamAppInfoFetcher::HandleFinished);
	if (!Runner->Start(User->SteamCmdPath.FilePath, FSteamCmdCommandLine::BuildAppInfoPrint(UserName, AppId), OutError))
	{
		Runner.Reset();
		return false;
	}
	return true;
}

void FSteamAppInfoFetcher::Cancel()
{
	if (Runner.IsValid())
	{
		Runner->Cancel();
	}
}

void FSteamAppInfoFetcher::HandleEvent(const FSteamCmdEvent& Event)
{
	switch (Event.Type)
	{
	case ESteamCmdEventType::Line:
		Lines.Add(Event.Text);
		break;
	case ESteamCmdEventType::GuardEmailRequested:
	case ESteamCmdEventType::GuardMobileRequested:
		bGuardRequested = true;
		Runner->Cancel();
		break;
	default:
		break;
	}
}

void FSteamAppInfoFetcher::HandleFinished(const FSteamCmdResult& Result)
{
	bool bSuccess = false;
	FText Message;
	FString FilePath;
	TSharedPtr<FJsonObject> AppInfo;

	if (bGuardRequested || Result.Outcome == ESteamCmdOutcome::NeedsPassword)
	{
		Message = LOCTEXT("NeedsLogin", "SteamCMD has no cached login. Use \"Open login terminal\" in the Publish tool, log in once and try again.");
	}
	else if (Result.Outcome == ESteamCmdOutcome::Canceled)
	{
		Message = LOCTEXT("Canceled", "Cancelled.");
	}
	else if (Result.Outcome == ESteamCmdOutcome::LoginFailed)
	{
		Message = FText::Format(LOCTEXT("LoginFailed", "Steam login failed: {0}"), FText::FromString(Result.Message));
	}
	// Exit code 0 with "Failed" is SteamCMD's harmless shutdown assertion ("ConfigStore ... is dirty"), which the parser's error pattern
	// catches. The block check below decides whether the run really worked.
	else if (Result.Outcome != ESteamCmdOutcome::Success && !(Result.Outcome == ESteamCmdOutcome::Failed && Result.ReturnCode == 0))
	{
		Message = FText::Format(LOCTEXT("Failed", "SteamCMD failed (exit code {0}). {1}"), Result.ReturnCode, FText::FromString(Result.Message));
	}
	else
	{
		const FString Block = ExtractAppBlock(Lines, AppId);
		if (Block.IsEmpty())
		{
			Message = FText::Format(LOCTEXT("NoBlock", "SteamCMD finished but printed no data for app {0}. Does the account have access to it?"), AppId);
		}
		else
		{
			FString ParseError;
			FString Json;
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
			if (!FSteamAppInfo::ParseKeyValues(Block, AppInfo, ParseError) || !FJsonSerializer::Serialize(AppInfo.ToSharedRef(), Writer))
			{
				Message = FText::Format(LOCTEXT("ParseFailed", "Could not read the app info SteamCMD printed: {0}"), FText::FromString(ParseError));
				AppInfo.Reset();
			}
			else
			{
				FilePath = GetOutputPath(AppId);
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
				if (FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					bSuccess = true;
					Message = FText::Format(LOCTEXT("Saved", "App info saved to {0}"), FText::FromString(FilePath));
				}
				else
				{
					Message = FText::Format(LOCTEXT("WriteFailed", "Could not write {0}"), FText::FromString(FilePath));
					FilePath.Reset();
					AppInfo.Reset();
				}
			}
		}
	}

	FinishedDelegate.Broadcast(bSuccess, Message, FilePath, AppInfo);
}

FString FSteamAppInfoFetcher::ExtractAppBlock(const TArray<FString>& Lines, int32 AppId)
{
	const FString Header = FString::Printf(TEXT("\"%d\""), AppId);

	// SteamCMD may prefix a line with "[2026-10-01 13:48:04] ". The block's first line carries it, the rest do not.
	auto StripTimestamp = [](const FString& Line)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.StartsWith(TEXT("[")))
		{
			int32 Close = INDEX_NONE;
			if (Trimmed.FindChar(TEXT(']'), Close) && Close <= 24)
			{
				Trimmed = Trimmed.Mid(Close + 1).TrimStart();
			}
		}
		return Trimmed;
	};

	int32 Start = INDEX_NONE;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (StripTimestamp(Lines[Index]) == Header)
		{
			Start = Index;
			break;
		}
	}
	if (Start == INDEX_NONE)
	{
		return FString();
	}

	// Count braces from the header on. Keys and values are quoted, but a quoted "{" is not a block, so only lines that are a lone brace count.
	int32 Depth = 0;
	bool bOpened = false;
	FString Result;
	for (int32 Index = Start; Index < Lines.Num(); ++Index)
	{
		const FString Trimmed = StripTimestamp(Lines[Index]);
		// The header line is written without its timestamp; the indented lines keep their indentation.
		Result += (Index == Start ? Trimmed : Lines[Index].TrimEnd()) + LINE_TERMINATOR;
		if (Trimmed == TEXT("{"))
		{
			++Depth;
			bOpened = true;
		}
		else if (Trimmed == TEXT("}"))
		{
			--Depth;
			if (bOpened && Depth <= 0)
			{
				return Result;
			}
		}
	}

	// Block never closed (cancelled or truncated output): better nothing than a file that looks complete.
	return FString();
}

#undef LOCTEXT_NAMESPACE
