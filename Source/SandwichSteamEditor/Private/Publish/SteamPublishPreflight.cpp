// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishPreflight.h"
#include "Dashboard/SSteamDashboardPanel.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Publish/SteamCmdOutputParser.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamVdfWriter.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPublishPreflight"

namespace
{
	FSteamValidationCheck MakeCheck(const TCHAR* Id, ESteamCheckSeverity Severity, const FText& Label, const FText& Detail)
	{
		FSteamValidationCheck Check;
		Check.Id = Id;
		Check.Severity = Severity;
		Check.Label = Label;
		Check.Detail = Detail;
		return Check;
	}
}

bool FSteamPublishPreflight::HasErrors(const TArray<FSteamValidationCheck>& Checks)
{
	return Checks.ContainsByPredicate([](const FSteamValidationCheck& Check) { return Check.Severity == ESteamCheckSeverity::Error; });
}

TArray<FSteamValidationCheck> FSteamPublishPreflight::Run(const FSteamPublishOptions& Options)
{
	TArray<FSteamValidationCheck> Checks;
	const USteamPublishSettings* Settings = USteamPublishSettings::Get();
	const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
	// bRun: something actually executes (pre/package/vdf/post steps). bPackage: UAT runs. bUpload: SteamCMD actually uploads.
	const bool bRun = !Options.bDryRun;
	const bool bPackage = bRun && !Options.bSkipPackaging;
	const bool bUpload = bRun && !Options.bSkipUpload;

	// App ID
	const int32 AppId = Settings->GetAppId();
	if (AppId <= 0)
	{
		Checks.Add(MakeCheck(TEXT("AppId"), ESteamCheckSeverity::Error, LOCTEXT("AppId", "Steam App ID"), LOCTEXT("AppIdMissing", "Not set. Enter it in Project Settings > Sandwich Steam.")));
	}
	else if (AppId == 480 && bUpload)
	{
		Checks.Add(MakeCheck(TEXT("AppId"), ESteamCheckSeverity::Warning, LOCTEXT("AppId", "Steam App ID"), LOCTEXT("AppIdSpacewar", "480 is Valve's Spacewar test app. You cannot upload builds to it; use your own App ID.")));
	}

	// Depots
	int32 EnabledDepots = 0;
	bool bDepotsValid = true;
	bool bMac = false;
	TSet<int32> SeenIds;
	for (const FSteamPublishDepot& Depot : Settings->Depots)
	{
		if (!Depot.bEnabled)
		{
			continue;
		}
		++EnabledDepots;
		bMac |= Depot.Platform == ESteamPublishPlatform::Mac;
		if (Depot.DepotId <= 0 || SeenIds.Contains(Depot.DepotId))
		{
			bDepotsValid = false;
		}
		SeenIds.Add(Depot.DepotId);
	}
	if (EnabledDepots == 0)
	{
		Checks.Add(MakeCheck(TEXT("Depots"), ESteamCheckSeverity::Error, LOCTEXT("Depots", "Depots"), LOCTEXT("NoDepot", "No enabled depot. Add one in the Setup page.")));
	}
	else if (!bDepotsValid)
	{
		Checks.Add(MakeCheck(TEXT("Depots"), ESteamCheckSeverity::Error, LOCTEXT("Depots", "Depots"), LOCTEXT("BadDepot", "A depot has no Depot ID or an ID is listed twice.")));
	}

	if (bRun)
	{
		// Unsaved assets and PIE (the cook reads the files on disk)
		if (bPackage)
		{
			if (GEditor && GEditor->PlayWorld)
			{
				Checks.Add(MakeCheck(TEXT("PIE"), ESteamCheckSeverity::Error, LOCTEXT("Pie", "Play In Editor"), LOCTEXT("PieRunning", "Stop Play In Editor before packaging.")));
			}

			TArray<UPackage*> Dirty;
			FEditorFileUtils::GetDirtyContentPackages(Dirty);
			FEditorFileUtils::GetDirtyWorldPackages(Dirty);
			if (!Dirty.IsEmpty())
			{
				FSteamValidationCheck Check = MakeCheck(TEXT("Unsaved"), ESteamCheckSeverity::Warning, LOCTEXT("Unsaved", "Unsaved assets"),
					FText::Format(LOCTEXT("UnsavedDetail", "{0} asset(s) have unsaved changes. The build is cooked from the saved files."), Dirty.Num()));
				Check.FixLabel = LOCTEXT("SaveAll", "Save all");
				Check.Fix = []() { FEditorFileUtils::SaveDirtyPackages(true, true, true); };
				Checks.Add(MoveTemp(Check));
			}

			if (bMac)
			{
#if !PLATFORM_MAC
				Checks.Add(MakeCheck(TEXT("MacHost"), ESteamCheckSeverity::Error, LOCTEXT("MacHost", "Mac depot"), LOCTEXT("MacHostDetail", "Mac builds can only be packaged on a Mac. Disable the Mac depot, or package on a Mac and use 'Upload staged build'.")));
#endif
			}

			// Multiple build targets (Target.cs), e.g. a Lyra-style project with a Client, a GameEOS and a Game target.
			// BuildCookRun needs -target to know which one to build; without it UAT either fails ambiguously or guesses.
			TArray<FString> TargetFiles;
			IFileManager::Get().FindFiles(TargetFiles, *(FPaths::ProjectDir() / TEXT("Source") / TEXT("*.Target.cs")), true, false);
			TArray<FString> TargetNames;
			for (const FString& File : TargetFiles)
			{
				TargetNames.Add(FPaths::GetBaseFilename(File, true).LeftChop(7)); // GetBaseFilename strips ".cs", LeftChop(7) strips ".Target"
			}
			const FString TargetName = Settings->TargetName.TrimStartAndEnd();
			if (TargetName.IsEmpty() && TargetNames.Num() > 1)
			{
				Checks.Add(MakeCheck(TEXT("Target"), ESteamCheckSeverity::Warning, LOCTEXT("Target", "Build target"),
					FText::Format(LOCTEXT("TargetAmbiguous", "This project has {0} build targets ({1}). Set 'Target Name' in the Setup page's Build settings, or BuildCookRun may pick the wrong one or fail with an ambiguous target error."),
						TargetNames.Num(), FText::FromString(FString::Join(TargetNames, TEXT(", "))))));
			}
			else if (!TargetName.IsEmpty() && !TargetNames.IsEmpty() && !TargetNames.Contains(TargetName))
			{
				Checks.Add(MakeCheck(TEXT("Target"), ESteamCheckSeverity::Warning, LOCTEXT("Target", "Build target"),
					FText::Format(LOCTEXT("TargetNotFound", "Target Name '{0}' does not match any Target.cs in Source/ ({1})."),
						FText::FromString(TargetName), FText::FromString(FString::Join(TargetNames, TEXT(", "))))));
			}
		}

		// Validator errors (missing OSS plugin, App ID, ini, ...)
		const TArray<FSteamValidationCheck> Setup = FSteamProjectValidator::Run();
		if (Setup.ContainsByPredicate([](const FSteamValidationCheck& Check) { return Check.Severity == ESteamCheckSeverity::Error; }))
		{
			Checks.Add(MakeCheck(TEXT("Validator"), ESteamCheckSeverity::Error, LOCTEXT("Validator", "Steam setup"), LOCTEXT("ValidatorDetail", "The Steam setup has errors. Fix them on the Steam Dashboard's status panel (Tools > Sandwich Steam > Steam Dashboard).")));
		}

		if (bUpload)
		{
			// SteamCMD
			if (User->SteamCmdPath.FilePath.IsEmpty() || !IFileManager::Get().FileExists(*User->SteamCmdPath.FilePath))
			{
				FSteamValidationCheck Check = MakeCheck(TEXT("SteamCmd"), ESteamCheckSeverity::Error, LOCTEXT("SteamCmd", "SteamCMD"),
					LOCTEXT("SteamCmdMissing", "SteamCMD was not found. Set its path (or download it) on the SteamCMD dashboard page."));
				Check.FixLabel = LOCTEXT("OpenDashboard", "Open dashboard");
				Check.Fix = []() { SandwichSteam::Editor::SandwichSteamDashboard(); };
				Checks.Add(MoveTemp(Check));
			}
			else if (!FSteamCmdCommandLine::IsValidUsername(User->SteamUsername.TrimStartAndEnd()))
			{
				FSteamValidationCheck Check = MakeCheck(TEXT("SteamUser"), ESteamCheckSeverity::Error, LOCTEXT("SteamUser", "Steam account"),
					LOCTEXT("SteamUserMissing", "Enter your Steam account name on the SteamCMD dashboard page (letters, digits and _ . - @ only)."));
				Check.FixLabel = LOCTEXT("OpenDashboard", "Open dashboard");
				Check.Fix = []() { SandwichSteam::Editor::SandwichSteamDashboard(); };
				Checks.Add(MoveTemp(Check));
			}

			// Branch
			if (Options.Branch.IsEmpty())
			{
				Checks.Add(MakeCheck(TEXT("Branch"), ESteamCheckSeverity::Warning, LOCTEXT("Branch", "Branch"), LOCTEXT("NoBranch", "No branch selected. The build is uploaded but not set live.")));
			}
			else if (!FSteamVdfWriter::CanSetLive(Options.Branch))
			{
				Checks.Add(MakeCheck(TEXT("Branch"), ESteamCheckSeverity::Info, LOCTEXT("Branch", "Branch"), LOCTEXT("DefaultBranch", "Steam does not allow setlive on the default branch. The build is uploaded; set it live on the partner site.")));
			}

			if (bMac)
			{
				Checks.Add(MakeCheck(TEXT("MacSign"), ESteamCheckSeverity::Warning, LOCTEXT("MacSign", "Mac build"), LOCTEXT("MacSignDetail", "Mac builds must be codesigned and notarized before they are uploaded.")));
			}
		}
	}

	Checks.Add(MakeCheck(TEXT("AppIdFile"), ESteamCheckSeverity::Info, LOCTEXT("AppIdFile", "steam_appid.txt"), LOCTEXT("AppIdFileDetail", "Always excluded from every depot.")));

	if (Checks.ContainsByPredicate([](const FSteamValidationCheck& Check) { return Check.Severity != ESteamCheckSeverity::Info; }) == false)
	{
		Checks.Insert(MakeCheck(TEXT("Ready"), ESteamCheckSeverity::Ok, LOCTEXT("Ready", "Ready"), LOCTEXT("ReadyDetail", "All checks passed.")), 0);
	}
	return Checks;
}

#undef LOCTEXT_NAMESPACE
