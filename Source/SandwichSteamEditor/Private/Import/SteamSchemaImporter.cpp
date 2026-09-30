// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Import/SteamSchemaImporter.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsManager.h"
#include "Import/SteamSchemaMerge.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "SandwichSteamEditor.h"
#include "ScopedTransaction.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SandwichSteamSchemaImporter"

namespace
{
	FText GetImportTitle()
	{
		return LOCTEXT("ImportTitle", "Import from Steam");
	}

	void ShowNotification(const FText& Text)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 8.0f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(SNotificationItem::CS_Success);
		}
	}

	/** Finds or creates Steam.Achievement.<ApiName> in the project's gameplay tag ini. */
	FGameplayTag FindOrCreateAchievementTag(FName ApiName)
	{
		const FString TagName = FSteamSchemaMerge::MakeAchievementTagName(ApiName);
		if (TagName.IsEmpty())
		{
			return FGameplayTag();
		}

		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagName), false);
		if (!Tag.IsValid())
		{
			IGameplayTagsEditorModule::Get().AddNewGameplayTagToINI(TagName, FString::Printf(TEXT("Achievement %s (imported from Steam)"), *ApiName.ToString()));
			Tag = Manager.RequestGameplayTag(FName(*TagName), false);
		}

		if (!Tag.IsValid())
		{
			UE_LOG(LogSandwichSteamEditor, Warning, TEXT("Import from Steam: could not create the gameplay tag %s."), *TagName);
		}
		return Tag;
	}
}

FString SandwichSteam::Editor::GetSchemaExportPath(int32 SteamAppId)
{
	return USteamToolSettings::Get()->GetDataDirectory() / FString::Printf(TEXT("Schema_%d.json"), SteamAppId);
}

void SandwichSteam::Editor::ImportFromSteam(USteamAppDefinition* Definition)
{
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	if (!Definition && Settings)
	{
		Definition = Settings->LoadAppDefinition();
	}

	if (!Definition)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoDefinition", "There is no Steam App Definition to import into. Create one (Content Browser > Add > Steam > Steam App Definition) and assign it in Project Settings > Plugins > Sandwich Steam."),
			GetImportTitle());
		return;
	}

	const int32 AppId = Settings ? Settings->SteamAppId : 0;
	const FString ExportPath = GetSchemaExportPath(AppId);

	FString Json;
	if (!FPaths::FileExists(ExportPath) || !FFileHelper::LoadFileToString(Json, *ExportPath))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("NoExport",
				"The schema export was not found:\n{0}\n\n1. Run the game in Standalone (Steam is not available inside Play In Editor).\n2. Type Steam.Debug.Show and click \"Export schema for the editor importer\" (or run the console command Steam.Stats.ExportSchema).\n3. Come back and import again."),
				FText::FromString(ExportPath)),
			GetImportTitle());
		return;
	}

	FSteamSchemaData Schema;
	FString Error;
	if (!FSteamSchemaMerge::ParseSchema(Json, Schema, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("ParseFailed", "Could not read {0}:\n{1}"), FText::FromString(ExportPath), FText::FromString(Error)), GetImportTitle());
		return;
	}

	if (Schema.AppId != 0 && Schema.AppId != AppId)
	{
		const EAppReturnType::Type Continue = FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(LOCTEXT("AppIdMismatch", "The export was written for App ID {0}, but the project uses {1}. Import it anyway?"), FText::AsNumber(Schema.AppId), FText::AsNumber(AppId)),
			GetImportTitle());
		if (Continue != EAppReturnType::Yes)
		{
			return;
		}
	}

	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNoCancel,
		FText::Format(LOCTEXT("ConfirmImport",
			"Steam reports {0} achievements. They are merged into {1} by API name: nothing is deleted and existing tags and progress settings are kept.\n\nCreate gameplay tags (Steam.Achievement.<ApiName>) in your project's gameplay tag ini for rows without a tag?\n\nYes: merge and create tags\nNo: merge without tags\nCancel: do nothing"),
			FText::AsNumber(Schema.Achievements.Num()), FText::FromString(Definition->GetName())),
		GetImportTitle());
	if (Answer == EAppReturnType::Cancel)
	{
		return;
	}

	FSteamSchemaMergeResult Result;
	{
		const FScopedTransaction Transaction(LOCTEXT("ImportTransaction", "Import Achievements from Steam"));
		Definition->Modify();

		TFunction<FGameplayTag(FName)> MakeTag;
		if (Answer == EAppReturnType::Yes)
		{
			MakeTag = &FindOrCreateAchievementTag;
		}

		Result = FSteamSchemaMerge::MergeAchievements(Definition->Achievements, Schema, MakeTag);
		Definition->MarkPackageDirty();
		Definition->PostEditChange();
	}

	for (const FName& Missing : Result.MissingInSteam)
	{
		UE_LOG(LogSandwichSteamEditor, Warning, TEXT("Import from Steam: achievement row '%s' is not reported by Steam (kept). Delete it by hand if it was removed in Steamworks."), *Missing.ToString());
	}

	ShowNotification(FText::Format(
		LOCTEXT("ImportDone", "Import from Steam: {0} added, {1} updated, {2} unchanged, {3} tags assigned, {4} not reported by Steam (kept). Save {5} to keep the changes."),
		FText::AsNumber(Result.Added.Num()), FText::AsNumber(Result.Updated), FText::AsNumber(Result.Unchanged),
		FText::AsNumber(Result.TagsAssigned), FText::AsNumber(Result.MissingInSteam.Num()), FText::FromString(Definition->GetName())));
}

#undef LOCTEXT_NAMESPACE
