// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SSteamCmdSetupPage.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Publish/SteamCmdDownloadJob.h"
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
	 * through TAttribute/lambdas, same idiom as the App Definition page's row counts.
	 */
	class SSteamCmdSetupPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSteamCmdSetupPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& /*InArgs*/)
		{
			UserDetails = MakeUserDetailsView();

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
											[this]() { if (Job.IsValid()) { Job->Cancel(); } },
											TAttribute<bool>::CreateLambda([this]() { return IsRunning(); }))
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									SNew(SBox).HeightOverride(6.f).Visibility_Lambda([this]() { return IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
									[
										SNew(SProgressBar).Percent_Lambda([this]() -> TOptional<float>
										{
											return ProgressValue >= 0.f ? TOptional<float>(ProgressValue) : TOptional<float>();
										})
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Visibility_Lambda([this]() { return IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
									.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())
									.Text_Lambda([this]() { return StepText; })
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

		virtual ~SSteamCmdSetupPage() override
		{
			if (Job.IsValid() && Job->IsRunning())
			{
				Job->Cancel();
			}
		}

		/** False while a download runs and the user declines to cancel it. Cancels the run when they accept. */
		bool CanCloseTab()
		{
			if (!IsRunning())
			{
				return true;
			}
			const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
				LOCTEXT("CloseWhileRunning", "A SteamCMD download is still running. Cancel it and close the tab?"));
			if (Answer == EAppReturnType::Yes)
			{
				Job->Cancel();
				return true;
			}
			return false;
		}

	private:
		bool IsRunning() const { return Job.IsValid() && Job->IsRunning(); }

		void OnDownloadClicked()
		{
			if (IsRunning())
			{
				return;
			}
			ProgressValue = -1.f;
			StepText = FText::GetEmpty();

			Job = MakeShared<FSteamCmdDownloadJob>();
			Job->OnProgress().AddSP(this, &SSteamCmdSetupPage::HandleProgress);
			Job->OnFinished().AddSP(this, &SSteamCmdSetupPage::HandleFinished);

			FString Error;
			if (!Job->Start(Error))
			{
				Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
				Job.Reset();
			}
		}

		void HandleProgress(float Progress, const FText& StepLabel)
		{
			ProgressValue = Progress;
			StepText = StepLabel;
		}

		void HandleFinished(bool bSuccess, const FText& Message, const FString& InstalledExePath)
		{
			Notify(Message, bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);

			if (bSuccess && !InstalledExePath.IsEmpty())
			{
				const FText Question = FText::Format(LOCTEXT("ApplyPathQuestion", "SteamCMD installed at {0}. Set it as the SteamCMD path in Sandwich Steam - Publish (User)?"), FText::FromString(InstalledExePath));
				if (FMessageDialog::Open(EAppMsgType::YesNo, Question) == EAppReturnType::Yes)
				{
					USteamPublishUserSettings* User = GetMutableDefault<USteamPublishUserSettings>();
					User->SteamCmdPath.FilePath = InstalledExePath;
					User->SaveConfig();
					if (UserDetails.IsValid())
					{
						UserDetails->ForceRefresh();
					}
				}
			}
		}

		TSharedPtr<FSteamCmdDownloadJob> Job;
		TSharedPtr<IDetailsView> UserDetails;
		float ProgressValue = -1.f;
		FText StepText;
	};
}

namespace SandwichSteam::Editor
{
	void RegisterSteamCmdDashboardPage()
	{
		const TSharedRef<TWeakPtr<SSteamCmdSetupPage>> PageRef = MakeShared<TWeakPtr<SSteamCmdSetupPage>>();

		FSteamDashboardPage Page;
		Page.Id = TEXT("SteamCmd");
		Page.Label = LOCTEXT("PageLabel", "SteamCMD");
		Page.Icon = FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16");
		Page.Order = 20;
		Page.BuildContent = [PageRef]() -> TSharedRef<SWidget>
		{
			const TSharedRef<SSteamCmdSetupPage> Panel = SNew(SSteamCmdSetupPage);
			*PageRef = Panel;
			return Panel;
		};
		Page.CanClose = [PageRef]()
		{
			const TSharedPtr<SSteamCmdSetupPage> Panel = PageRef->Pin();
			return !Panel.IsValid() || Panel->CanCloseTab();
		};

		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
