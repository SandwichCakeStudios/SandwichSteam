// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SSteamCmdSetupPage.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Dom/JsonObject.h"
#include "Publish/SteamAppInfoData.h"
#include "Publish/SteamCmdDownloadJob.h"
#include "Publish/SteamCmdSetupService.h"
#include "Publish/SteamPublishSettings.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDashboardSteamCmd"

namespace
{
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick, TAttribute<bool> bEnabled = true)
	{
		return SNew(SButton)
			.ToolTipText(ToolTip)
			.IsEnabled(bEnabled)
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
			];
	}

	TSharedRef<IDetailsView> MakeUserDetailsView()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs Args;
		Args.bAllowSearch = false;
		Args.bHideSelectionTip = true;
		Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;

		const TSharedRef<IDetailsView> View = PropertyModule.CreateDetailView(Args);
		USteamPublishUserSettings* Object = GetMutableDefault<USteamPublishUserSettings>();
		View->SetObject(Object);
		// Same behaviour this view had on the Publish page's Setup tab: every edit is written straight to the per user config file.
		View->OnFinishedChangingProperties().AddLambda([Object](const FPropertyChangedEvent&) { Object->SaveConfig(); });
		return View;
	}

	void Notify(const FText& Text, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 8.0f;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	FText BuildStatusText()
	{
		const TOptional<FSteamCmdManifest> Manifest = SandwichSteam::Editor::ReadSteamCmdManifest(SandwichSteam::Editor::GetSteamCmdRoot());
		if (Manifest.IsSet())
		{
			return FText::Format(LOCTEXT("StatusInstalled", "Installed {0} from {1}."), FText::AsDate(Manifest->DownloadedUtc), FText::FromString(Manifest->Url));
		}

		const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
		if (User->SteamCmdPath.FilePath.IsEmpty())
		{
			return LOCTEXT("StatusNotInstalled", "Not installed. Press Download SteamCMD below, or fill in the path yourself if you already have it.");
		}
		if (!IFileManager::Get().FileExists(*User->SteamCmdPath.FilePath))
		{
			return LOCTEXT("StatusPathMissing", "The SteamCMD path below does not point at a file that exists.");
		}
		return LOCTEXT("StatusPathSet", "SteamCMD path is set (not downloaded with this tool).");
	}

	/**
	 * Status line, Download SteamCMD button (progress bar + step text while running, Cancel) and the moved
	 * "Your account" details view. Rebuilds nothing on a timer: the status line and details view read live state
	 * through TAttribute/lambdas, same idiom as the App Definition page's row counts. The jobs themselves live in
	 * FSteamCmdSetupService (shared with the Setup page); this page runs them in Confirm mode and asks before applying.
	 */
	class SSteamCmdSetupPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSteamCmdSetupPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& /*InArgs*/)
		{
			UserDetails = MakeUserDetailsView();

			FSteamCmdSetupService& Service = FSteamCmdSetupService::Get();
			Service.OnDownloadFinished().AddSP(this, &SSteamCmdSetupPage::HandleDownloadFinished);
			Service.OnAppInfoFinished().AddSP(this, &SSteamCmdSetupPage::HandleAppInfoFinished);

			ChildSlot
			[
				SNew(SBox).Padding(4.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(STextBlock).Text(LOCTEXT("PageTitle", "SteamCMD")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
						[
							SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).AutoWrapText(true).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)).Text_Lambda([]() { return BuildStatusText(); })
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
									[
										MakeButton(LOCTEXT("Download", "Download SteamCMD"),
											LOCTEXT("DownloadTip", "Downloads, extracts and self-updates SteamCMD for this editor's platform (Windows, Mac or Linux). See SteamCMD offical documentation for the manual steps this replaces."),
											[this]() { OnDownloadClicked(); },
											TAttribute<bool>::CreateLambda([this]() { return !IsRunning(); }))
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										MakeButton(LOCTEXT("Cancel", "Cancel"), LOCTEXT("CancelTip", "Stops the running download or extraction."),
											[]() { FSteamCmdSetupService::Get().CancelDownload(); },
											TAttribute<bool>::CreateLambda([this]() { return IsRunning(); }))
										]
									+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0.f, 0.f, 0.f)
									[
										SNew(SButton)
										.ToolTipText(LOCTEXT("AppInfoTip", "Runs SteamCMD app_info_print for the App ID with the cached login and saves the result as JSON in the Publish directory (AppInfo folder). Then offers to add the depots and branches to the Publish settings. Needs the SteamCMD path and Steam account below, and a login cached by the Publish tool."))
										.IsEnabled_Lambda([this]() { return !IsRunning(); })
										.OnClicked_Lambda([this]()
										{
											OnAppInfoClicked();
											return FReply::Handled();
										})
										[
											SNew(STextBlock)
											.Text_Lambda([this]() { return IsAppInfoRunning() ? LOCTEXT("AppInfoCancel", "Cancel Fetch App Info") : LOCTEXT("AppInfo", "Fetch App Info"); })
											.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									SNew(SBox).HeightOverride(6.f).Visibility_Lambda([this]() { return IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
									[
										SNew(SProgressBar).Percent_Lambda([]() -> TOptional<float>
										{
											const float Progress = FSteamCmdSetupService::Get().GetDownloadProgress();
											return Progress >= 0.f ? TOptional<float>(Progress) : TOptional<float>();
										})
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Visibility_Lambda([this]() { return IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
									.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())
									.Text_Lambda([]() { return FSteamCmdSetupService::Get().GetDownloadStep(); })
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 2.f)
						[
							SNew(STextBlock).Text(LOCTEXT("UserSection", "Your account (not shared)")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).MaxDesiredHeight(240.f)
							[
								UserDetails.ToSharedRef()
							]
						]
					]
				]
			];
		}

	private:
		static bool IsRunning() { return FSteamCmdSetupService::Get().IsDownloading(); }
		static bool IsAppInfoRunning() { return FSteamCmdSetupService::Get().IsFetchingAppInfo(); }

		void OnAppInfoClicked()
		{
			FSteamCmdSetupService& Service = FSteamCmdSetupService::Get();
			// One button: Fetch App Info, and Cancel Fetch App Info while it runs.
			if (Service.IsFetchingAppInfo())
			{
				Service.CancelAppInfoFetch();
				return;
			}

			SandwichSteam::Editor::ConfirmAndFetchAppInfo(ESteamCmdTaskMode::Confirm);
		}

		/** The service already notified the result. Fetches started by the Setup page (Automatic) were applied there. */
		void HandleAppInfoFinished(bool bSuccess, const FString& FilePath, const TSharedPtr<FJsonObject>& AppInfoJson, ESteamCmdTaskMode Mode)
		{
			if (!bSuccess || Mode != ESteamCmdTaskMode::Confirm)
			{
				return;
			}

			USteamPublishSettings* Settings = GetMutableDefault<USteamPublishSettings>();
			const int32 AppId = Settings->GetAppId();
			FSteamAppInfoData Data;
			FString Error;
			if (!FSteamAppInfo::Extract(AppInfoJson, AppId, Data, Error))
			{
				Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
				return;
			}

			// Always asked, even when nothing is new: the dialog doubles as the summary of what Steam has.
			const FSteamAppInfoMerge Merge = FSteamAppInfo::Preview(*Settings, Data);
			const FText Question = FText::Format(LOCTEXT("ApplyAppInfoQuestion",
				"Steam lists {0} depot(s) and {1} branch(es) for app {2}.\n\nNew depots: {3}\nNew branches: {4}\n\nAdd them to Sandwich Steam - Publish (Depots, Branches)? Existing entries are kept as they are.\n\nSaved to {5}"),
				Data.Depots.Num(), Data.Branches.Num(), AppId,
				FText::FromString(Merge.NewDepotIds.IsEmpty() ? TEXT("none") : FString::JoinBy(Merge.NewDepotIds, TEXT(", "), [](int32 Id) { return FString::FromInt(Id); })),
				FText::FromString(Merge.NewBranches.IsEmpty() ? TEXT("none") : FString::Join(Merge.NewBranches, TEXT(", "))),
				FText::FromString(FilePath));
			if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
			{
				return;
			}

			FSteamAppInfo::Apply(*Settings, Data);
			Settings->TryUpdateDefaultConfigFile();
			Notify(FText::Format(LOCTEXT("AppliedAppInfo", "Added {0} depot(s) and {1} branch(es) to the Publish settings."), Merge.NewDepotIds.Num(), Merge.NewBranches.Num()), SNotificationItem::CS_Success);
		}

		void OnDownloadClicked()
		{
			SandwichSteam::Editor::ConfirmAndDownloadSteamCmd(ESteamCmdTaskMode::Confirm);
		}

		/** The service already notified the result. Downloads started by the Setup page (Automatic) set the path there. */
		void HandleDownloadFinished(bool bSuccess, const FString& InstalledExePath, ESteamCmdTaskMode Mode, bool bPathApplied)
		{
			bool bRefresh = bPathApplied;
			if (bSuccess && Mode == ESteamCmdTaskMode::Confirm && !InstalledExePath.IsEmpty())
			{
				const FText Question = FText::Format(LOCTEXT("ApplyPathQuestion", "SteamCMD installed at {0}. Set it as the SteamCMD path in Sandwich Steam - Publish (User)?"), FText::FromString(InstalledExePath));
				if (FMessageDialog::Open(EAppMsgType::YesNo, Question) == EAppReturnType::Yes)
				{
					USteamPublishUserSettings* User = GetMutableDefault<USteamPublishUserSettings>();
					User->SteamCmdPath.FilePath = InstalledExePath;
					User->SaveConfig();
					bRefresh = true;
				}
			}
			if (bRefresh && UserDetails.IsValid())
			{
				UserDetails->ForceRefresh();
			}
		}

		TSharedPtr<IDetailsView> UserDetails;
	};
}

namespace SandwichSteam::Editor
{
	void RegisterSteamCmdDashboardPage()
	{
		FSteamDashboardPage Page;
		Page.Id = TEXT("SteamCmd");
		Page.Label = LOCTEXT("PageLabel", "SteamCMD");
		Page.Icon = FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16");
		Page.Order = 20;
		Page.BuildContent = []() -> TSharedRef<SWidget> { return SNew(SSteamCmdSetupPage); };
		// The jobs belong to the shared service: closing the tab while one runs asks to cancel it.
		Page.CanClose = []() { return FSteamCmdSetupService::Get().ConfirmCancelForClose(); };

		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
