// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishSettingsCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HAL/FileManager.h"
#include "Publish/SteamCmdOutputParser.h"
#include "Publish/SteamCmdSetupService.h"
#include "Publish/SteamPublishActions.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamPublishVdf.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPublishCustomization"

namespace
{
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.ToolTipText(ToolTip)
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label)
			];
	}

	FText GetDepotProblems()
	{
		const USteamPublishSettings* Settings = USteamPublishSettings::Get();
		TArray<FText> Lines;
		if (Settings->GetAppId() <= 0)
		{
			Lines.Add(LOCTEXT("ProblemAppId", "Steam App ID is not set (Project Settings > Sandwich Steam)."));
		}

		int32 Enabled = 0;
		TSet<int32> Seen;
		for (const FSteamPublishDepot& Depot : Settings->Depots)
		{
			if (!Depot.bEnabled)
			{
				continue;
			}
			++Enabled;
			if (Depot.DepotId <= 0)
			{
				Lines.Add(LOCTEXT("ProblemDepotId", "A depot has no Depot ID."));
			}
			else if (Seen.Contains(Depot.DepotId))
			{
				Lines.Add(FText::Format(LOCTEXT("ProblemDuplicate", "Depot {0} is listed twice."), Depot.DepotId));
			}
			Seen.Add(Depot.DepotId);
		}
		if (Enabled == 0)
		{
			Lines.Add(LOCTEXT("ProblemNoDepot", "No enabled depot."));
		}

		if (Lines.IsEmpty())
		{
			return LOCTEXT("NoProblems", "Depot setup looks valid.");
		}
		return FText::Join(FText::FromString(TEXT("\n")), Lines);
	}

	FText GetSteamCmdStatus()
	{
		const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
		if (User->SteamCmdPath.FilePath.IsEmpty())
		{
			return LOCTEXT("StatusNoPath", "SteamCMD path is not set.");
		}
		if (!IFileManager::Get().FileExists(*User->SteamCmdPath.FilePath))
		{
			return LOCTEXT("StatusMissing", "SteamCMD was not found at this path.");
		}
		if (!FSteamCmdCommandLine::IsValidUsername(User->SteamUsername.TrimStartAndEnd()))
		{
			return LOCTEXT("StatusNoUser", "Enter your Steam account name (letters, digits and _ . - @ only).");
		}
		return LOCTEXT("StatusReady", "Ready. Log in once with the terminal, then use Test login. No password is stored by this tool.");
	}
}

TSharedRef<IDetailCustomization> FSteamPublishSettingsCustomization::MakeInstance()
{
	return MakeShared<FSteamPublishSettingsCustomization>();
}

void FSteamPublishSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Status", LOCTEXT("StatusCategory", "Status"), ECategoryPriority::Important);
	Category.AddCustomRow(LOCTEXT("StatusFilter", "status app id depot dry run vdf"))
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Text_Lambda([]() { return FText::Format(LOCTEXT("AppIdLine", "Steam App ID: {0} (from Sandwich Steam)"), USteamPublishSettings::Get()->GetAppId()); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([]() { return GetDepotProblems(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 6.f, 4.f)
			[
				MakeButton(LOCTEXT("DryRun", "Generate VDF (dry run)"),
					FText::Format(LOCTEXT("DryRunTip", "Writes the app and depot build scripts for the first branch into {0}. Nothing is uploaded."), FText::FromString(SandwichSteam::Publish::GetPublishDir())),
					[]() { SandwichSteam::Editor::GenerateVdfDryRun(); })
			]
		]
	];
}

TSharedRef<IDetailCustomization> FSteamPublishUserSettingsCustomization::MakeInstance()
{
	return MakeShared<FSteamPublishUserSettingsCustomization>();
}

void FSteamPublishUserSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Login", LOCTEXT("LoginCategory", "Login"), ECategoryPriority::Important);
	Category.AddCustomRow(LOCTEXT("LoginFilter", "steamcmd login terminal test"))
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([]() { return GetSteamCmdStatus(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 6.f, 4.f)
			[
				MakeButton(LOCTEXT("OpenTerminal", "Open login terminal"),
					LOCTEXT("OpenTerminalTip", "Opens a terminal running steamcmd +login <user>. Type your password and Steam Guard code there once; SteamCMD caches the login. The login is checked when you come back to the editor."),
					[]() { FSteamCmdSetupService::Get().BeginTerminalLogin(); })
			]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 6.f, 4.f)
			[
				MakeButton(LOCTEXT("TestLogin", "Test login"),
					LOCTEXT("TestLoginTip", "Runs steamcmd +login <user> +quit with the cached login and shows the result. A Steam Guard prompt opens a dialog."),
					[]() { SandwichSteam::Editor::TestSteamCmdLogin(); })
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
