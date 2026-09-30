// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SSteamStatusPanel.h"
#include "Core/SteamToolSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "ISettingsModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Settings/SteamConfigureAction.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamStatusPanel"

namespace
{
	const TCHAR* GetStatusBrushName(ESteamCheckSeverity Severity)
	{
		switch (Severity)
		{
		case ESteamCheckSeverity::Error:
			return TEXT("SandwichSteam.Status.Error");
		case ESteamCheckSeverity::Warning:
			return TEXT("SandwichSteam.Status.Warning");
		case ESteamCheckSeverity::Info:
			return TEXT("SandwichSteam.Status.Info");
		default:
			return TEXT("SandwichSteam.Status.Ok");
		}
	}

	void ShowNotification(const FText& Text, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 6.0f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	/** Folder of the running editor executable (Engine/Binaries/<Platform> of the engine version in use). */
	FString GetEditorBinariesDir()
	{
		return FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir());
	}

	/** <Project>/Binaries/<Platform>: where the game executable of this project lives during development. */
	FString GetProjectBinariesDir()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory());
	}

	/** Writes steam_appid.txt into Directory. Steam reads it when the game was not started by the Steam client. */
	void CreateSteamAppIdFile(const FString& Directory)
	{
		const USteamToolSettings* Settings = USteamToolSettings::Get();
		if (!Settings || Settings->SteamAppId <= 0)
		{
			ShowNotification(LOCTEXT("AppIdFileNoId", "Set a Steam App ID first."), SNotificationItem::CS_Fail);
			return;
		}

		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString FilePath = Directory / TEXT("steam_appid.txt");
		if (FFileHelper::SaveStringToFile(FString::FromInt(Settings->SteamAppId), *FilePath, FFileHelper::EEncodingOptions::ForceAnsi))
		{
			ShowNotification(FText::Format(LOCTEXT("AppIdFileDone", "Wrote {0}. For development only; never ship it."), FText::FromString(FilePath)), SNotificationItem::CS_Success);
		}
		else
		{
			ShowNotification(FText::Format(LOCTEXT("AppIdFileFailed", "Could not write {0}. Check that the folder is writable."), FText::FromString(FilePath)), SNotificationItem::CS_Fail);
		}
	}

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
				SNew(STextBlock).Text(Label).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
			];
	}

	/** Same fixed size for every Setup status action button (25% larger than the default button), stacked vertically. */
	TSharedRef<SWidget> MakeActionButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick)
	{
		return SNew(SBox)
			.WidthOverride(300.f)
			.HeightOverride(50.f)
			[
				SNew(SButton)
				.ToolTipText(ToolTip)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(10.f, 5.f))
				.OnClicked_Lambda([OnClick]()
				{
					OnClick();
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					.AutoWrapText(true)
					.Justification(ETextJustify::Center)
				]
			];
	}

	/** Opens Project Settings > Plugins > Sandwich Steam. */
	void SandwichSteamProjectSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer("Project", "Plugins", USteamToolSettings::Get()->GetSectionName());
		}
	}

	constexpr float StatusDotSize = 14.f;
}

void SSteamStatusPanel::Construct(const FArguments& /*InArgs*/)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card"))
		.Padding(10.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SImage).Image(FSteamToolStyle::Get().GetBrush("SandwichSteam.Icon20"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("StripTitle", "Setup status"))
					.Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.3f))
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(Rows, SVerticalBox)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ActionsTitle", "Actions"))
				.Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.3f))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 4.f)
				[
					MakeActionButton(LOCTEXT("Recheck", "Re-check"), LOCTEXT("RecheckTip", "Runs the validation again."), [this]() { Refresh(); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 4.f)
				[
					MakeActionButton(LOCTEXT("ShowLog", "Show in Message Log"), LOCTEXT("ShowLogTip", "Writes the result to the Message Log (Sandwich Steam) and opens it."), [this]()
					{
						FSteamProjectValidator::WriteToMessageLog(Checks, true);
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 4.f)
				[
					MakeActionButton(LOCTEXT("Configure", "Configure Steam..."), LOCTEXT("ConfigureTip", "Shows the changes to DefaultEngine.ini (platform service, Steam subsystem, SteamDevAppId) and applies them."), [this]()
					{
						SandwichSteam::Editor::ConfigureSteam();
						Refresh();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 4.f)
				[
					MakeActionButton(LOCTEXT("ProjectSettings", "Project Settings..."), LOCTEXT("ProjectSettingsTip", "Opens Project Settings > Plugins > Sandwich Steam (App ID, Data Directory, Features)."), []()
					{
						SandwichSteamProjectSettings();
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 4.f)
				[
					MakeActionButton(LOCTEXT("AppIdFileEngine", "Create steam_appid.txt in Engine Binaries"),
						FText::Format(LOCTEXT("AppIdFileEngineTip", "Writes steam_appid.txt into the folder of the running editor executable ({0}), so games started from the editor (Standalone) use your App ID. Applies to this engine version only. Development only."), FText::FromString(GetEditorBinariesDir())),
						[]() { CreateSteamAppIdFile(GetEditorBinariesDir()); })
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeActionButton(LOCTEXT("AppIdFileProject", "Create steam_appid.txt in Project Binaries"),
						FText::Format(LOCTEXT("AppIdFileProjectTip", "Writes steam_appid.txt into {0}, next to the game executable of this project (development builds started outside Steam). Development only."), FText::FromString(GetProjectBinariesDir())),
						[]() { CreateSteamAppIdFile(GetProjectBinariesDir()); })
				]
			]
		]
	];

	ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SSteamStatusPanel::HandleObjectChanged);
	DefinitionChangedHandle = USteamToolSettings::OnAppDefinitionChanged().AddSP(this, &SSteamStatusPanel::RequestRefresh);

	Refresh();
}

SSteamStatusPanel::~SSteamStatusPanel()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
	USteamToolSettings::OnAppDefinitionChanged().Remove(DefinitionChangedHandle);
}

void SSteamStatusPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bRefreshRequested)
	{
		Refresh();
	}
}

void SSteamStatusPanel::HandleObjectChanged(UObject* Object, FPropertyChangedEvent& /*Event*/)
{
	if (Object == GetDefault<USteamToolSettings>())
	{
		RequestRefresh();
	}
}

void SSteamStatusPanel::RequestRefresh()
{
	bRefreshRequested = true;
}

void SSteamStatusPanel::Refresh()
{
	bRefreshRequested = false;
	Checks = FSteamProjectValidator::Run();

	Rows->ClearChildren();
	for (const FSteamValidationCheck& Check : Checks)
	{
		Rows->AddSlot().AutoHeight().Padding(0.f, 5.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SBox).WidthOverride(StatusDotSize).HeightOverride(StatusDotSize)
				[
					SNew(SImage).Image(FSteamToolStyle::Get().GetBrush(GetStatusBrushName(Check.Severity)))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Check.Label)
				.Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.25f))
				.ToolTipText(Check.Detail)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Visibility(Check.Fix ? EVisibility::Visible : EVisibility::Collapsed)
				.ToolTipText(LOCTEXT("FixTip", "Applies the fix and checks again."))
				.OnClicked_Lambda([this, Fix = Check.Fix]()
				{
					Fix();
					Refresh();
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Check.FixLabel)
				]
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
