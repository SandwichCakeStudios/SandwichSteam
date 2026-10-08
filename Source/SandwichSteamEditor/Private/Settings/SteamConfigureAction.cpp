// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SteamConfigureAction.h"
#include "Core/SteamToolSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformFileManager.h"
#include "ISourceControlModule.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SandwichSteamEditor.h"
#include "Settings/SteamIniWriter.h"
#include "SourceControlHelpers.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SandwichSteamConfigure"

namespace
{
	/** One ini file of the project and what has to change in it. */
	struct FIniTarget
	{
		FString FileName;
		FString Path;
		FString Existing;
		TArray<FSteamIniEntry> Entries;
		TArray<FSteamIniChange> Changes;

		bool HasChanges() const
		{
			return Changes.ContainsByPredicate([](const FSteamIniChange& Change)
			{
				return Change.Change != ESteamIniChange::Unchanged;
			});
		}
	};

	void ShowNotification(const FText& Text, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 6.0f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	FText DescribeChanges(const TArray<FIniTarget>& Targets)
	{
		FString Text;
		for (const FIniTarget& Target : Targets)
		{
			if (!Target.HasChanges())
			{
				continue;
			}

			Text += FString::Printf(TEXT("Config/%s\n"), *Target.FileName);
			for (const FSteamIniChange& Change : Target.Changes)
			{
				if (Change.Change == ESteamIniChange::Unchanged)
				{
					continue;
				}

				if (Change.Change == ESteamIniChange::Added)
				{
					Text += FString::Printf(TEXT("+ [%s] %s=%s\n"), *Change.Entry.Section, *Change.Entry.Key, *Change.Entry.Value);
				}
				else
				{
					Text += FString::Printf(TEXT("~ [%s] %s: %s -> %s\n"), *Change.Entry.Section, *Change.Entry.Key, *Change.OldValue, *Change.Entry.Value);
				}
			}
			Text += TEXT("\n");
		}
		return FText::Format(LOCTEXT("ConfirmBody", "Sandwich Steam will change:\n\n{0}Apply these changes?"), FText::FromString(Text));
	}

	/** Loads the file (when it exists) and works out the changes. False when the file exists but cannot be read. */
	bool LoadTarget(FIniTarget& Target, const TCHAR* FileName, TArray<FSteamIniEntry> Entries)
	{
		Target.FileName = FileName;
		Target.Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / FileName);
		Target.Entries = MoveTemp(Entries);

		if (FPaths::FileExists(Target.Path) && !FFileHelper::LoadFileToString(Target.Existing, *Target.Path))
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("ReadFailed", "Could not read {0}."), FText::FromString(Target.Path)),
				LOCTEXT("ConfigureTitle", "Configure Steam"));
			return false;
		}

		Target.Changes = FSteamIniWriter::Diff(Target.Existing, Target.Entries);
		return true;
	}

	bool WriteTarget(const FIniTarget& Target)
	{
		const FString NewIni = FSteamIniWriter::Apply(Target.Existing, Target.Entries);

		if (FPaths::FileExists(Target.Path))
		{
			if (ISourceControlModule::Get().IsEnabled())
			{
				USourceControlHelpers::CheckOutOrAddFile(Target.Path, /*bSilent*/ true);
			}

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (PlatformFile.IsReadOnly(*Target.Path))
			{
				PlatformFile.SetReadOnly(*Target.Path, false);
			}
		}

		if (!FFileHelper::SaveStringToFile(NewIni, *Target.Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogSandwichSteamEditor, Error, TEXT("Configure Steam: could not write %s"), *Target.Path);
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("WriteFailed", "Could not write {0}. Check that the file is writable."), FText::FromString(Target.Path)),
				LOCTEXT("ConfigureTitle", "Configure Steam"));
			return false;
		}

		UE_LOG(LogSandwichSteamEditor, Log, TEXT("Configure Steam: updated %s"), *Target.Path);
		return true;
	}
}

bool SandwichSteam::Editor::WantsSessionsIni()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("SandwichSteamSessions"));
}

bool SandwichSteam::Editor::WantsVoiceIni()
{
	return FModuleManager::Get().IsModuleLoaded(TEXT("SandwichSteamVoice"));
}

FSteamNetworkTuning SandwichSteam::Editor::MakeNetworkTuning(const USteamToolSettings& Settings)
{
	FSteamNetworkTuning Tuning;
	Tuning.BandwidthPerClient = Settings.NetBandwidthPerClient;
	Tuning.TotalBandwidth = Settings.TotalNetBandwidth;
	Tuning.MinDynamicBandwidth = Settings.MinDynamicBandwidth;
	Tuning.InitialConnectTimeout = Settings.InitialConnectTimeout;
	return Tuning;
}

TArray<FSteamIniEntry> SandwichSteam::Editor::BuildEngineIniEntries(const USteamToolSettings& Settings)
{
	TArray<FSteamIniEntry> Entries = FSteamIniWriter::BuildRequiredEntries(Settings.SteamAppId, WantsSessionsIni(), WantsVoiceIni(), Settings.bWriteRelaunchInSteamOff);
	if (Settings.bWriteNetworkTuning)
	{
		Entries.Append(FSteamIniWriter::BuildNetworkEngineEntries(MakeNetworkTuning(Settings)));
	}
	return Entries;
}

TArray<FSteamIniEntry> SandwichSteam::Editor::BuildGameIniEntries(const USteamToolSettings& Settings)
{
	TArray<FSteamIniEntry> Entries = FSteamIniWriter::BuildGameEntries(WantsVoiceIni());
	if (Settings.bWriteNetworkTuning)
	{
		Entries.Append(FSteamIniWriter::BuildNetworkGameEntries(MakeNetworkTuning(Settings)));
	}
	return Entries;
}

void SandwichSteam::Editor::ConfigureSteam()
{
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	if (!Settings || Settings->SteamAppId <= 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("InvalidAppId", "Set a Steam App ID greater than 0 in Project Settings > Plugins > Sandwich Steam first. Use 480 (Spacewar) for testing."),
			LOCTEXT("ConfigureTitle", "Configure Steam"));
		return;
	}

	TArray<FIniTarget> Targets;
	Targets.AddDefaulted(2);
	if (!LoadTarget(Targets[0], TEXT("DefaultEngine.ini"), BuildEngineIniEntries(*Settings))
		|| !LoadTarget(Targets[1], TEXT("DefaultGame.ini"), BuildGameIniEntries(*Settings)))
	{
		return;
	}

	if (!Targets[0].HasChanges() && !Targets[1].HasChanges())
	{
		ShowNotification(LOCTEXT("AlreadyConfigured", "Steam is already configured (DefaultEngine.ini and DefaultGame.ini)."), SNotificationItem::CS_Success);
		return;
	}

	if (FMessageDialog::Open(EAppMsgType::YesNo, DescribeChanges(Targets), LOCTEXT("ConfigureTitle", "Configure Steam")) != EAppReturnType::Yes)
	{
		return;
	}

	for (const FIniTarget& Target : Targets)
	{
		if (Target.HasChanges() && !WriteTarget(Target))
		{
			return;
		}
	}

	ShowNotification(LOCTEXT("Configured", "Config updated. Restart the editor for the Steam settings to take effect."), SNotificationItem::CS_Success);
}

#undef LOCTEXT_NAMESPACE
