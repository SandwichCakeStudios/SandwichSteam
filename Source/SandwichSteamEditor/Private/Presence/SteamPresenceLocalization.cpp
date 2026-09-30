// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Presence/SteamPresenceLocalization.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Features/Utility/SteamLanguage.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "SandwichSteamEditor.h"
#include "Presence/SteamPresenceVdfWriter.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPresenceLocalization"

namespace
{
	FText GetTitle()
	{
		return LOCTEXT("Title", "Generate Rich Presence Localization");
	}

	/** A status row that has text. */
	struct FPresenceRow
	{
		FString Token;
		FText Text;
		TSet<FString> Keys;
	};

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

FString SandwichSteam::Editor::GetPresenceLocalizationPath(int32 SteamAppId)
{
	return USteamToolSettings::Get()->GetDataDirectory() / FString::Printf(TEXT("richpresence_%d.vdf"), SteamAppId);
}

void SandwichSteam::Editor::GeneratePresenceLocalization(USteamAppDefinition* Definition)
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

	TArray<FPresenceRow> Rows;
	int32 WithoutText = 0;
	bool bAllInvariant = true;
	for (const FSteamPresenceDef& Def : Definition->Presence)
	{
		if (Def.Token.IsEmpty())
		{
			continue;
		}

		if (Def.LocalizedText.IsEmpty())
		{
			++WithoutText;
			continue;
		}

		FPresenceRow& Row = Rows.AddDefaulted_GetRef();
		Row.Token = Def.Token;
		Row.Text = Def.LocalizedText;
		for (const FName Key : Def.ExtraKeys)
		{
			Row.Keys.Add(Key.ToString());
		}
		bAllInvariant &= Def.LocalizedText.IsCultureInvariant();
	}

	if (Rows.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoRows", "The Steam App Definition has no presence status with a token and a text. Add rows to its Presence array first."), GetTitle());
		return;
	}

	FInternationalization& I18N = FInternationalization::Get();
	const FString OriginalLanguage = I18N.GetCurrentLanguage()->GetName();

	TArray<FString> UnknownKeys;
	TArray<FSteamPresenceLanguage> Languages;
	{
		const TArray<FString> SteamLanguages = SandwichSteam::GetSteamLanguageNames();
		FScopedSlowTask Progress(SteamLanguages.Num(), LOCTEXT("Progress", "Reading the text in every Steam language..."));
		Progress.MakeDialogDelayed(0.5f);

		for (const FString& SteamLanguage : SteamLanguages)
		{
			Progress.EnterProgressFrame();

			FString Culture;
			const bool bEnglish = SteamLanguage == TEXT("english");
			if (!SandwichSteam::SteamLanguageToCulture(SteamLanguage, Culture) || !I18N.GetCulture(Culture).IsValid())
			{
				continue;
			}

			// Text that is not localized reads the same in every language: skip the (slow) culture switches.
			if (bAllInvariant && !bEnglish)
			{
				continue;
			}

			I18N.SetCurrentLanguage(Culture);

			FSteamPresenceLanguage Language;
			Language.Language = SteamLanguage;
			for (const FPresenceRow& Row : Rows)
			{
				const FString Text = Row.Text.ToString();

				// Steam falls back to English for tokens a language does not have, so untranslated text is left out.
				if (!bEnglish && Text == Row.Text.BuildSourceString())
				{
					continue;
				}

				Language.Tokens.Emplace(Row.Token, FSteamPresenceVdfWriter::ConvertPlaceholders(Text, Row.Keys, &UnknownKeys));
			}

			if (!Language.Tokens.IsEmpty())
			{
				Languages.Add(MoveTemp(Language));
			}
		}
	}
	I18N.SetCurrentLanguage(OriginalLanguage);

	const FString Content = FSteamPresenceVdfWriter::Build(Languages);
	if (Content.IsEmpty())
	{
		ShowNotification(LOCTEXT("NothingToWrite", "No text was found for any Steam language. Nothing was written."), FString(), false);
		return;
	}

	const FString Path = GetPresenceLocalizationPath(Settings ? Settings->SteamAppId : 0);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);
	if (!FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		ShowNotification(FText::Format(LOCTEXT("WriteFailed", "Could not write {0}."), FText::FromString(Path)), FString(), false);
		return;
	}

	for (const FString& Key : UnknownKeys)
	{
		UE_LOG(LogSandwichSteamEditor, Warning, TEXT("Rich presence localization: the text uses {%s}, which is not listed in the Extra Keys of its status."), *Key);
	}

	FText Summary = FText::Format(LOCTEXT("Written", "Wrote {0} status(es) in {1} language(s) to {2}. Upload it on the Steamworks partner site (App Admin > Community > Rich Presence)."),
		FText::AsNumber(Rows.Num()), FText::AsNumber(Languages.Num()), FText::FromString(FPaths::GetCleanFilename(Path)));
	if (!UnknownKeys.IsEmpty())
	{
		Summary = FText::Format(LOCTEXT("WrittenUnknown", "{0} Warning: {1} placeholder(s) are not listed in Extra Keys (see the Output Log)."), Summary, FText::AsNumber(UnknownKeys.Num()));
	}
	if (WithoutText > 0)
	{
		Summary = FText::Format(LOCTEXT("WrittenSkipped", "{0} {1} status(es) without text were skipped."), Summary, FText::AsNumber(WithoutText));
	}

	UE_LOG(LogSandwichSteamEditor, Log, TEXT("Rich presence localization written to %s"), *Path);
	ShowNotification(Summary, Path, true);
}

#undef LOCTEXT_NAMESPACE
