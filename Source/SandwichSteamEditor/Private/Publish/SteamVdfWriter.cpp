// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamVdfWriter.h"

namespace
{
	void AddPair(FString& Out, int32 Indent, const FString& Key, const FString& Value)
	{
		Out.Append(FString::ChrN(Indent, TEXT('\t')));
		Out += FSteamVdfWriter::Quote(Key) + TEXT(" ") + FSteamVdfWriter::Quote(Value) + TEXT("\n");
	}

	void OpenBlock(FString& Out, int32 Indent, const FString& Key)
	{
		const FString Tabs = FString::ChrN(Indent, TEXT('\t'));
		Out += Tabs + FSteamVdfWriter::Quote(Key) + TEXT("\n") + Tabs + TEXT("{\n");
	}

	void CloseBlock(FString& Out, int32 Indent)
	{
		Out += FString::ChrN(Indent, TEXT('\t')) + TEXT("}\n");
	}
}

bool FSteamVdfWriter::CanSetLive(const FString& BranchName)
{
	return !BranchName.IsEmpty() && !BranchName.Equals(TEXT("default"), ESearchCase::IgnoreCase);
}

FString FSteamVdfWriter::GetDepotFileName(int32 DepotId)
{
	return FString::Printf(TEXT("depot_build_%d.vdf"), DepotId);
}

FString FSteamVdfWriter::GetAppFileName(int32 AppId)
{
	return FString::Printf(TEXT("app_build_%d.vdf"), AppId);
}

FString FSteamVdfWriter::NormalizePath(const FString& Path)
{
	return Path.Replace(TEXT("\\"), TEXT("/"));
}

FString FSteamVdfWriter::Quote(const FString& Text)
{
	FString Escaped = Text.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
	return TEXT("\"") + Escaped + TEXT("\"");
}

FString FSteamVdfWriter::BuildApp(const FSteamVdfApp& App)
{
	FString Out;
	OpenBlock(Out, 0, TEXT("AppBuild"));
	AddPair(Out, 1, TEXT("AppID"), FString::FromInt(App.AppId));
	AddPair(Out, 1, TEXT("Desc"), App.Description);
	AddPair(Out, 1, TEXT("Preview"), App.bPreview ? TEXT("1") : TEXT("0"));

	FString BuildOutput = NormalizePath(App.BuildOutput);
	if (!BuildOutput.IsEmpty() && !BuildOutput.EndsWith(TEXT("/")))
	{
		BuildOutput += TEXT("/");
	}
	AddPair(Out, 1, TEXT("BuildOutput"), BuildOutput);

	if (!App.ContentRoot.IsEmpty())
	{
		AddPair(Out, 1, TEXT("ContentRoot"), NormalizePath(App.ContentRoot));
	}
	if (CanSetLive(App.SetLiveBranch))
	{
		AddPair(Out, 1, TEXT("SetLive"), App.SetLiveBranch);
	}

	OpenBlock(Out, 1, TEXT("Depots"));
	for (const int32 DepotId : App.DepotIds)
	{
		AddPair(Out, 2, FString::FromInt(DepotId), GetDepotFileName(DepotId));
	}
	CloseBlock(Out, 1);
	CloseBlock(Out, 0);
	return Out;
}

FString FSteamVdfWriter::BuildDepot(const FSteamVdfDepot& Depot)
{
	FString Out;
	OpenBlock(Out, 0, TEXT("DepotBuildConfig"));
	AddPair(Out, 1, TEXT("DepotID"), FString::FromInt(Depot.DepotId));
	AddPair(Out, 1, TEXT("ContentRoot"), NormalizePath(Depot.ContentRoot));

	for (const FSteamVdfFileMapping& Mapping : Depot.FileMappings)
	{
		OpenBlock(Out, 1, TEXT("FileMapping"));
		AddPair(Out, 2, TEXT("LocalPath"), NormalizePath(Mapping.LocalPath));
		AddPair(Out, 2, TEXT("DepotPath"), NormalizePath(Mapping.DepotPath));
		AddPair(Out, 2, TEXT("Recursive"), Mapping.bRecursive ? TEXT("1") : TEXT("0"));
		CloseBlock(Out, 1);
	}
	for (const FString& Exclusion : Depot.FileExclusions)
	{
		AddPair(Out, 1, TEXT("FileExclusion"), NormalizePath(Exclusion));
	}
	CloseBlock(Out, 0);
	return Out;
}

FString FSteamVdfWriter::FormatDescription(const FString& Template, const TMap<FString, FString>& Tokens)
{
	FString Result = Template;
	for (const TPair<FString, FString>& Token : Tokens)
	{
		Result = Result.Replace(*(TEXT("{") + Token.Key + TEXT("}")), *Token.Value);
	}
	return Result;
}
