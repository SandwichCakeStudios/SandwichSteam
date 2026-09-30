// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Input/SteamInputActionsFile.h"
#include "Core/SteamInputRules.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Input/SteamInputVdfWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "SandwichSteamEditor.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SandwichSteamInputActionsFile"

namespace
{
	FText GetTitle()
	{
		return LOCTEXT("Title", "Generate Steam Input Actions File");
	}

	void ShowNotification(const FText& Text, const FString& FilePath, bool bSuccess)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 12.0f;
		if (bSuccess && !FilePath.IsEmpty())
		{
			Info.HyperlinkText = LOCTEXT("ShowFile", "Show file");
			Info.Hyperlink = FSimpleDelegate::CreateLambda([FilePath]()
			{
				FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FilePath));
			});
		}

		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}
}

FString SandwichSteam::Editor::GetSteamInputActionsPath(int32 SteamAppId)
{
	return USteamToolSettings::Get()->GetDataDirectory() / FString::Printf(TEXT("game_actions_%d.vdf"), SteamAppId);
}

void SandwichSteam::Editor::GenerateSteamInputActions(USteamAppDefinition* Definition)
{
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	if (!Definition && Settings)
	{
		Definition = Settings->LoadAppDefinition();
	}

	if (!Definition)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoDefinition", "No Steam App Definition is assigned. Assign one in Project Settings > Plugins > Sandwich Steam."), GetTitle());
		return;
	}

	TArray<FSteamInputVdfSet> Sets;
	int32 Skipped = 0;
	for (const FSteamInputActionSetDef& Def : Definition->InputSets)
	{
		if (SandwichSteam::Input::CheckName(Def.SteamSetName.IsNone() ? FString() : Def.SteamSetName.ToString(), /*bIsAction*/ false) != SandwichSteam::Input::ENameIssue::None)
		{
			++Skipped;
			continue;
		}

		FSteamInputVdfSet& Set = Sets.AddDefaulted_GetRef();
		Set.Name = Def.SteamSetName.ToString();
		Set.bLayer = Def.bLayer;
#if WITH_EDITORONLY_DATA
		Set.Title = Def.EditorDisplayName;
#endif

		for (const FSteamInputActionDef& ActionDef : Def.Actions)
		{
			if (SandwichSteam::Input::CheckName(ActionDef.SteamName.IsNone() ? FString() : ActionDef.SteamName.ToString(), /*bIsAction*/ true) != SandwichSteam::Input::ENameIssue::None)
			{
				++Skipped;
				continue;
			}

			FSteamInputVdfAction& Action = Set.Actions.AddDefaulted_GetRef();
			Action.Name = ActionDef.SteamName.ToString();
			Action.Kind = ActionDef.Kind;
			Action.InputMode = ActionDef.InputMode.IsNone() ? FString() : ActionDef.InputMode.ToString();
#if WITH_EDITORONLY_DATA
			Action.Title = ActionDef.EditorDisplayName;
#endif
		}
	}

	const FString Content = FSteamInputVdfWriter::Build(Sets);
	if (Content.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoRows", "The Steam App Definition has no Input Set with a valid name. Add rows to its Input Sets array first."), GetTitle());
		return;
	}

	const FString Path = GetSteamInputActionsPath(Settings ? Settings->SteamAppId : 0);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);
	if (!FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		ShowNotification(FText::Format(LOCTEXT("WriteFailed", "Could not write {0}."), FText::FromString(Path)), FString(), false);
		return;
	}

	FText Summary = FText::Format(LOCTEXT("Written", "Wrote {0} action set(s) to {1}. Upload it on the Steamworks partner site (Steam Input > Edit Steam Input Configurations), or set it as the Action Manifest in Project Settings > Plugins > Sandwich Steam - Input to test."),
		FText::AsNumber(Sets.Num()), FText::FromString(FPaths::GetCleanFilename(Path)));
	if (Skipped > 0)
	{
		Summary = FText::Format(LOCTEXT("WrittenSkipped", "{0} {1} entrie(s) with an invalid name were skipped (see Data Validation)."), Summary, FText::AsNumber(Skipped));
	}

	UE_LOG(LogSandwichSteamEditor, Log, TEXT("Steam Input action file written to %s"), *Path);
	ShowNotification(Summary, Path, true);
}

#undef LOCTEXT_NAMESPACE
