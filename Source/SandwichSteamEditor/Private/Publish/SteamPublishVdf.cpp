// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishVdf.h"
#include "Core/SteamToolSettings.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Publish/SteamVdfWriter.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPublishVdf"

namespace SandwichSteam::Publish
{
	namespace
	{
		FString MakeAbsolute(FString Directory)
		{
			if (FPaths::IsRelative(Directory))
			{
				Directory = FPaths::ProjectDir() / Directory;
			}
			Directory = FPaths::ConvertRelativePathToFull(Directory);
			FPaths::NormalizeDirectoryName(Directory);
			return Directory;
		}
	}

	FString GetPublishDir()
	{
		const FString Custom = USteamPublishSettings::Get()->PublishDirectory.Path.TrimStartAndEnd();
		if (!Custom.IsEmpty())
		{
			return MakeAbsolute(Custom);
		}
		return USteamToolSettings::Get()->GetDataDirectory() / TEXT("Publish");
	}

	FString GetStagingDir()
	{
		const FString Custom = USteamPublishSettings::Get()->StagingDirectory.Path.TrimStartAndEnd();
		if (Custom.IsEmpty())
		{
			// ProjectSavedDir() can itself be relative to the engine base dir ("../../../Proj/Saved/"); resolve it as such,
			// never against ProjectDir(), or the ".." climb past the drive root and SteamCMD rejects the path.
			FString Staging = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("StagedBuilds"));
			FPaths::NormalizeDirectoryName(Staging);
			return Staging;
		}
		return MakeAbsolute(Custom);
	}

	FString GetStagedFolderName(ESteamPublishPlatform Platform)
	{
		switch (Platform)
		{
		case ESteamPublishPlatform::Mac:
			return TEXT("Mac");
		case ESteamPublishPlatform::Linux:
			return TEXT("Linux");
		default:
			return TEXT("Windows");
		}
	}

	FString ResolveContentRoot(const FSteamPublishDepot& Depot)
	{
		FString Root = Depot.ContentRoot.TrimStartAndEnd();
		if (Root.IsEmpty())
		{
			Root = GetStagingDir() / GetStagedFolderName(Depot.Platform);
		}
		return FSteamVdfWriter::NormalizePath(MakeAbsolute(Root));
	}

	bool WriteVdfFiles(const USteamPublishSettings& Settings, const FString& BranchName, FVdfFiles& OutFiles, FString& OutError)
	{
		const int32 AppId = Settings.GetAppId();
		if (AppId <= 0)
		{
			OutError = LOCTEXT("NoAppId", "Set the Steam App ID in Project Settings > Sandwich Steam.").ToString();
			return false;
		}

		FSteamVdfApp App;
		App.AppId = AppId;
		App.SetLiveBranch = BranchName;

		TArray<FSteamVdfDepot> Depots;
		TSet<int32> SeenIds;
		for (const FSteamPublishDepot& Source : Settings.Depots)
		{
			if (!Source.bEnabled)
			{
				continue;
			}
			if (Source.DepotId <= 0)
			{
				OutError = LOCTEXT("BadDepotId", "A depot has no Depot ID. Enter the ID from the partner site or disable the depot.").ToString();
				return false;
			}
			if (SeenIds.Contains(Source.DepotId))
			{
				OutError = FText::Format(LOCTEXT("DuplicateDepot", "Depot {0} is listed twice."), Source.DepotId).ToString();
				return false;
			}
			SeenIds.Add(Source.DepotId);

			FSteamVdfDepot Depot;
			Depot.DepotId = Source.DepotId;
			Depot.ContentRoot = ResolveContentRoot(Source);
			for (const FSteamPublishFileMapping& Mapping : Source.FileMappings)
			{
				Depot.FileMappings.Add({ Mapping.LocalPath, Mapping.DepotPath, Mapping.bRecursive });
			}
			if (Depot.FileMappings.IsEmpty())
			{
				Depot.FileMappings.Add(FSteamVdfFileMapping());
			}
			Depot.FileExclusions = Source.FileExclusions;
			// The development App ID file must never reach players (edge case E-40).
			Depot.FileExclusions.AddUnique(TEXT("steam_appid.txt"));

			if (!IFileManager::Get().DirectoryExists(*Depot.ContentRoot))
			{
				OutFiles.Warnings.Add(FText::Format(LOCTEXT("MissingContent", "Depot {0}: content folder does not exist yet ({1}). Package first."), Source.DepotId, FText::FromString(Depot.ContentRoot)).ToString());
			}

			App.DepotIds.Add(Depot.DepotId);
			Depots.Add(MoveTemp(Depot));
		}

		if (Depots.IsEmpty())
		{
			OutError = LOCTEXT("NoDepots", "No enabled depot. Add one in Project Settings > Sandwich Steam - Publish.").ToString();
			return false;
		}

		FString ConfigName = StaticEnum<ESteamPublishConfig>()->GetNameStringByValue(static_cast<int64>(Settings.PackageConfig));
		TMap<FString, FString> Tokens;
		Tokens.Add(TEXT("Project"), FApp::GetProjectName());
		Tokens.Add(TEXT("Config"), ConfigName);
		Tokens.Add(TEXT("Branch"), BranchName);
		Tokens.Add(TEXT("Date"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")));
		App.Description = FSteamVdfWriter::FormatDescription(Settings.BuildDescriptionTemplate, Tokens);

		const FString Dir = GetPublishDir();
		OutFiles.BuildOutputDir = FSteamVdfWriter::NormalizePath(Dir / TEXT("Output"));
		App.BuildOutput = OutFiles.BuildOutputDir;

		IFileManager::Get().MakeDirectory(*OutFiles.BuildOutputDir, true);

		for (const FSteamVdfDepot& Depot : Depots)
		{
			const FString Path = Dir / FSteamVdfWriter::GetDepotFileName(Depot.DepotId);
			if (!FFileHelper::SaveStringToFile(FSteamVdfWriter::BuildDepot(Depot), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutError = FText::Format(LOCTEXT("WriteFailed", "Could not write {0}."), FText::FromString(Path)).ToString();
				return false;
			}
			OutFiles.DepotVdfPaths.Add(FSteamVdfWriter::NormalizePath(Path));
		}

		const FString AppPath = Dir / FSteamVdfWriter::GetAppFileName(AppId);
		if (!FFileHelper::SaveStringToFile(FSteamVdfWriter::BuildApp(App), *AppPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FText::Format(LOCTEXT("WriteFailed", "Could not write {0}."), FText::FromString(AppPath)).ToString();
			return false;
		}
		OutFiles.AppVdfPath = FSteamVdfWriter::NormalizePath(AppPath);
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
