// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SSteamPublishPanel.h"
#include "Containers/Ticker.h"
#include "Dashboard/SteamConfirmDialog.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/PlatformProcess.h"
#include "IDetailsView.h"
#include "ISettingsModule.h"
#include "Core/SteamToolSettings.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Publish/SteamPublishActions.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamPublishVdf.h"
#include "Publish/SteamVdfWriter.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPublishPanel"

namespace
{
	const FName PublishTabName(TEXT("SandwichSteamPublishTab"));
	constexpr int32 MaxLogLines = 3000;

	const TCHAR* GetSeverityBrush(ESteamCheckSeverity Severity)
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

	const TCHAR* GetStepBrush(ESteamPublishStepState State)
	{
		switch (State)
		{
		case ESteamPublishStepState::Running:
			return TEXT("SandwichSteam.Status.Info");
		case ESteamPublishStepState::Done:
			return TEXT("SandwichSteam.Status.Ok");
		case ESteamPublishStepState::Failed:
			return TEXT("SandwichSteam.Status.Error");
		default:
			return TEXT("SandwichSteam.Status.Idle");
		}
	}

	FSlateColor GetLogColor(ESteamLogSeverity Severity)
	{
		switch (Severity)
		{
		case ESteamLogSeverity::Error:
			return FSlateColor(FLinearColor(0.95f, 0.35f, 0.35f));
		case ESteamLogSeverity::Warning:
			return FSlateColor(FLinearColor(0.95f, 0.72f, 0.25f));
		default:
			return FSlateColor::UseForeground();
		}
	}

	void Notify(const FText& Text, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 8.0f;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

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

	TSharedRef<SWidget> MakeRequirementRow(const FText& Title, const FText& Detail)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Title).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(Detail).AutoWrapText(true).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
			];
	}

	TSharedRef<IDetailsView> MakeDetailsView(UObject* Object)
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs Args;
		Args.bAllowSearch = false;
		Args.bHideSelectionTip = true;
		Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;

		const TSharedRef<IDetailsView> View = PropertyModule.CreateDetailView(Args);
		View->SetObject(Object);
		// Same behaviour as Project Settings: every edit is written to the shared, committed config file.
		View->OnFinishedChangingProperties().AddLambda([Object](const FPropertyChangedEvent&) { Object->TryUpdateDefaultConfigFile(); });
		return View;
	}
}

void SSteamPublishPanel::Construct(const FArguments& InArgs)
{
	for (const ESteamPublishStepId Step : FSteamPublishJob::GetAllSteps())
	{
		StepStates.Add(Step, ESteamPublishStepState::Pending);
	}
	StatusText = LOCTEXT("StatusIdle", "Ready. Pick a branch and press Publish, or Dry run to only write the build scripts.");

	SharedDetails = MakeDetailsView(GetMutableDefault<USteamPublishSettings>());

	RefreshBranches();
	RefreshHistory();

	ChildSlot
	[
		SNew(SVerticalBox)
		// Header
		+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 10.f, 12.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage).Image(FSteamToolStyle::Get().GetBrush("SandwichSteam.Publish40"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("Title", "Steam Publish")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([]() { return FText::Format(LOCTEXT("AppLine", "App {0}  |  files: {1}"), USteamPublishSettings::Get()->GetAppId(), FText::FromString(SandwichSteam::Publish::GetPublishDir())); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				MakePageToggle(0, LOCTEXT("PagePublish", "Publish"))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				MakePageToggle(1, LOCTEXT("PageSetup", "Setup"))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				MakePageToggle(2, LOCTEXT("PageHistory", "History"))
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(12.f, 0.f, 12.f, 12.f)
		[
			SAssignNew(Pages, SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return ActivePage; })
			+ SWidgetSwitcher::Slot()
			[
				BuildPublishPage()
			]
			+ SWidgetSwitcher::Slot()
			[
				BuildSetupPage()
			]
			+ SWidgetSwitcher::Slot()
			[
				BuildHistoryPage()
			]
		]
	];

	ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SSteamPublishPanel::HandleObjectChanged);
}

SSteamPublishPanel::~SSteamPublishPanel()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
	if (Job.IsValid() && Job->IsRunning())
	{
		Job->Cancel();
	}
}

void SSteamPublishPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// UAT can print thousands of lines: the list refreshes once per frame at most.
	if (bLogDirty && LogList.IsValid())
	{
		bLogDirty = false;
		LogList->RequestListRefresh();
		if (!LogLines.IsEmpty())
		{
			LogList->RequestScrollIntoView(LogLines.Last());
		}
	}
}

bool SSteamPublishPanel::CanCloseTab()
{
	if (!IsRunning())
	{
		return true;
	}
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo,
		LOCTEXT("CloseWhileRunning", "A publish is still running. Cancel it and close the tab?"));
	if (Answer == EAppReturnType::Yes)
	{
		Job->Cancel();
		return true;
	}
	return false;
}

TSharedRef<SWidget> SSteamPublishPanel::MakePageToggle(int32 Page, const FText& Label)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, Page]() { return ActivePage == Page ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this, Page](ECheckBoxState)
		{
			ActivePage = Page;
			if (Page == 2)
			{
				RefreshHistory();
			}
		})
		[
			SNew(STextBlock).Text(Label).Margin(FMargin(10.f, 3.f))
		];
}

TSharedRef<SWidget> SSteamPublishPanel::BuildPublishPage()
{
	// Steps row
	const TSharedRef<SHorizontalBox> StepsRow = SNew(SHorizontalBox);
	for (const ESteamPublishStepId Step : FSteamPublishJob::GetAllSteps())
	{
		StepsRow->AddSlot().AutoWidth().Padding(0.f, 0.f, 16.f, 0.f).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
				[
					SNew(SImage).Image_Lambda([this, Step]() { return FSteamToolStyle::Get().GetBrush(GetStepBrush(StepStates.FindRef(Step))); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FSteamPublishJob::GetStepLabel(Step)).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
			]
		];
	}

	return SNew(SVerticalBox)
		// Controls
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock).Text(LOCTEXT("Branch", "Branch")).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)
					[
						SNew(SBox).MinDesiredWidth(140.f)
						[
							SAssignNew(BranchCombo, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&BranchOptions)
							.InitiallySelectedItem(SelectedBranch)
							.IsEnabled_Lambda([this]() { return !IsRunning(); })
							.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
							{
								return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f));
							})
							.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type)
							{
								if (Item.IsValid())
								{
									SelectedBranch = Item;
								}
							})
							[
								SNew(STextBlock).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)).Text_Lambda([this]()
								{
									return SelectedBranch.IsValid() ? FText::FromString(*SelectedBranch) : LOCTEXT("NoBranchOption", "(no branch)");
								})
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)
					[
						SNew(SCheckBox)
						.IsEnabled_Lambda([this]() { return !IsRunning(); })
						.ToolTipText(LOCTEXT("SkipPackageTip", "Upload the folder that is already staged (see Staging Directory) without packaging again."))
						.IsChecked_Lambda([this]() { return bSkipPackaging ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bSkipPackaging = State == ECheckBoxState::Checked; })
						[
							SNew(STextBlock).Text(LOCTEXT("SkipPackage", "Upload staged build (skip packaging)")).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 16.f, 0.f)
					[
						SNew(SCheckBox)
						.IsEnabled_Lambda([this]() { return !IsRunning(); })
						.ToolTipText(LOCTEXT("LiveCodingTip", "UAT cannot package while Live Coding is enabled. When checked, Live Coding is switched off for the packaging step and restored afterwards. Uncheck to manage Live Coding yourself."))
						.IsChecked_Lambda([this]() { return bDisableLiveCoding ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bDisableLiveCoding = State == ECheckBoxState::Checked; })
						[
							SNew(STextBlock).Text(LOCTEXT("LiveCoding", "Pause Live Coding while packaging")).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNullWidget::NullWidget
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						MakeButton(LOCTEXT("DryRun", "Dry run"), LOCTEXT("DryRunTip", "Runs the checks and writes the app and depot build scripts. Nothing is packaged or uploaded."),
							[this]() { OnPublishClicked(true); }, TAttribute<bool>::CreateLambda([this]() { return !IsRunning(); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						MakeButton(LOCTEXT("TestBuild", "Test build (no upload)"), LOCTEXT("TestBuildTip", "Runs pre steps, packages with UAT, writes the build scripts and runs post steps. Never calls SteamCMD - use this to verify packaging before spending an upload."),
							[this]() { OnPublishClicked(false, true); }, TAttribute<bool>::CreateLambda([this]() { return !IsRunning(); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						MakeButton(LOCTEXT("Publish", "Publish"), LOCTEXT("PublishTip", "Package the game, write the build scripts and upload the build with SteamCMD."),
							[this]() { OnPublishClicked(false); }, TAttribute<bool>::CreateLambda([this]() { return !IsRunning(); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
					[
						MakeButton(LOCTEXT("Cancel", "Cancel"), LOCTEXT("CancelTip", "Stops the running process."),
							[this]()
							{
								if (Job.IsValid())
								{
									Job->Cancel();
								}
							}, TAttribute<bool>::CreateLambda([this]() { return IsRunning(); }))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					StepsRow
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 14.f, 12.f, 0.f)
				[
					// 50% larger than the other panel text (1.25 * 1.5).
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.875f))
					.Text_Lambda([this]() { return StatusText; })
					.ColorAndOpacity_Lambda([this]()
					{
						switch (StatusSeverity)
						{
						case ESteamCheckSeverity::Error:
							return FSlateColor(FLinearColor(0.95f, 0.35f, 0.35f));
						case ESteamCheckSeverity::Ok:
							return FSlateColor(FLinearColor(0.35f, 0.8f, 0.45f));
						default:
							return FSlateColor::UseForeground();
						}
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 10.f, 12.f, 10.f)
				[
					SNew(SBox).HeightOverride(12.f).Visibility_Lambda([this]() { return IsRunning() ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SNew(SProgressBar).Percent_Lambda([this]() -> TOptional<float>
						{
							return ProgressValue >= 0.f ? TOptional<float>(ProgressValue) : TOptional<float>();
						})
					]
				]
				// Pre-flight results, under the progress bar with the same padding
				+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 0.f, 12.f, 14.f)
				[
					SAssignNew(ChecksBox, SVerticalBox)
				]
			]
		]
		// Deployment requirements (a static checklist; the automated checks below cover what can be checked from here)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.AreaTitle(LOCTEXT("Requirements", "Steam deployment requirements (developer checklist)"))
			.BodyContent()
			[
				SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
					[
						MakeRequirementRow(LOCTEXT("ReqAppId", "Valid Steam App ID"), LOCTEXT("ReqAppIdDetail", "Checked automatically below (\"Steam App ID\")."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
					[
						MakeRequirementRow(LOCTEXT("ReqDepot", "Configured depot"), LOCTEXT("ReqDepotDetail", "The Steamworks partner site must have at least one depot set up. Checked automatically below (\"Depots\")."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
					[
						MakeRequirementRow(LOCTEXT("ReqPermissions", "Account permissions"), LOCTEXT("ReqPermissionsDetail", "Not checked automatically: on the partner site (App Admin > Users), this Steam account needs 'Edit App Metadata' and 'Publish App Changes To Steam' for this App ID and depot."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
					[
						MakeRequirementRow(LOCTEXT("ReqConnectivity", "SteamCMD connectivity"), LOCTEXT("ReqConnectivityDetail", "SteamCMD must reach the Steam servers. Use 'Test login' on the SteamCMD dashboard page; also checked automatically below (\"SteamCMD\")."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
					[
						MakeRequirementRow(LOCTEXT("ReqPackaging", "Successful packaging"), LOCTEXT("ReqPackagingDetail", "The project must package without errors. Use 'Test build (no upload)' above to verify this without spending an upload."))
					]
				]
			]
		]
		// Log
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Log", "Log")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				MakeButton(LOCTEXT("CopyLog", "Copy log"), LOCTEXT("CopyLogTip", "Copies the log to the clipboard."), [this]()
				{
					FString All;
					for (const TSharedPtr<FLogLine>& Line : LogLines)
					{
						All += Line->Text + LINE_TERMINATOR;
					}
					FPlatformApplicationMisc::ClipboardCopy(*All);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				MakeButton(LOCTEXT("ClearLog", "Clear log"), LOCTEXT("ClearLogTip", "Clears the log shown here. The log files in the publish directory are not touched."), [this]()
				{
					LogLines.Reset();
					bLogDirty = true;
					if (LogList.IsValid())
					{
						LogList->RequestListRefresh();
					}
				}, TAttribute<bool>::CreateLambda([this]() { return !IsRunning(); }))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				MakeButton(LOCTEXT("OpenFolder", "Open folder"), LOCTEXT("OpenFolderTip", "Opens the publish directory (scripts, logs, history)."), []()
				{
					const FString Dir = SandwichSteam::Publish::GetPublishDir();
					IFileManager::Get().MakeDirectory(*Dir, true);
					FPlatformProcess::ExploreFolder(*Dir);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				MakeButton(LOCTEXT("BuildsPage", "Builds page"), LOCTEXT("BuildsPageTip", "Opens the SteamPipe builds page of this app on the partner site."), []()
				{
					FPlatformProcess::LaunchURL(*FString::Printf(TEXT("https://partner.steamgames.com/apps/builds/%d"), USteamPublishSettings::Get()->GetAppId()), nullptr, nullptr);
				})
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(4.f)
			[
				SAssignNew(LogList, SListView<TSharedPtr<FLogLine>>)
				.ListItemsSource(&LogLines)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow_Lambda([](TSharedPtr<FLogLine> Line, const TSharedRef<STableViewBase>& Owner)
				{
					return SNew(STableRow<TSharedPtr<FLogLine>>, Owner)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Line->Text))
						.AutoWrapText(true)
						.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Mono", 9), 1.25f))
						.ColorAndOpacity(GetLogColor(Line->Severity))
					];
				})
			]
		];
}

TSharedRef<SWidget> SSteamPublishPanel::BuildSetupPage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("SetupIntro", "Depots, branches and packaging are shared with your team (DefaultEditor.ini). Changes are saved as you edit. The App ID and the data folder are set in Sandwich Steam. SteamCMD path and account are on the dashboard's SteamCMD page."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(LOCTEXT("SandwichSteamSettings", "Sandwich Steam settings"), LOCTEXT("SandwichSteamSettingsTip", "Opens Project Settings > Plugins > Sandwich Steam (App ID, Data Directory)."), []()
				{
					if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
					{
						SettingsModule->ShowViewer("Project", "Plugins", USteamToolSettings::Get()->GetSectionName());
					}
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("SharedSection", "Depots, branches, packaging (shared)")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SharedDetails.ToSharedRef()
		];
}

TSharedRef<SWidget> SSteamPublishPanel::BuildHistoryPage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)).ColorAndOpacity(FSlateColor::UseSubduedForeground()).Text(LOCTEXT("HistoryHint", "Uploads made with this tool, newest first."))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(LOCTEXT("RefreshHistory", "Refresh"), LOCTEXT("RefreshHistoryTip", "Reads the history file again."), [this]() { RefreshHistory(); })
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(4.f)
			[
				SAssignNew(HistoryList, SListView<TSharedPtr<FSteamPublishHistoryEntry>>)
				.ListItemsSource(&HistoryEntries)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow_Lambda([](TSharedPtr<FSteamPublishHistoryEntry> Entry, const TSharedRef<STableViewBase>& Owner)
				{
					const FText Build = Entry->BuildId > 0 ? FText::FromString(FString::Printf(TEXT("Build %lld"), Entry->BuildId)) : LOCTEXT("NoBuildId", "no BuildID");
					return SNew(STableRow<TSharedPtr<FSteamPublishHistoryEntry>>, Owner)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f, 8.f, 0.f)
						[
							SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
							[
								SNew(SImage).Image(FSteamToolStyle::Get().GetBrush(Entry->bSuccess ? TEXT("SandwichSteam.Status.Ok") : TEXT("SandwichSteam.Status.Error")))
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 2.f, 12.f, 2.f)
						[
							SNew(STextBlock).Text(FText::AsDateTime(Entry->Time)).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 2.f, 12.f, 2.f)
						[
							SNew(STextBlock).Text(FText::Format(LOCTEXT("HistoryBranch", "branch {0}"), Entry->Branch.IsEmpty() ? FText::FromString(TEXT("-")) : FText::FromString(Entry->Branch))).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 2.f, 12.f, 2.f)
						[
							SNew(STextBlock).Text(Build).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString(Entry->Message)).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					];
				})
			]
		];
}

void SSteamPublishPanel::RefreshBranches()
{
	const FString Previous = SelectedBranch.IsValid() ? *SelectedBranch : FString();

	BranchOptions.Reset();
	SelectedBranch.Reset();
	for (const FSteamPublishBranch& Branch : USteamPublishSettings::Get()->Branches)
	{
		const FString Name = Branch.Name.TrimStartAndEnd();
		if (!Name.IsEmpty())
		{
			BranchOptions.Add(MakeShared<FString>(Name));
			if (Name == Previous)
			{
				SelectedBranch = BranchOptions.Last();
			}
		}
	}
	if (!SelectedBranch.IsValid() && !BranchOptions.IsEmpty())
	{
		SelectedBranch = BranchOptions[0];
	}

	if (BranchCombo.IsValid())
	{
		BranchCombo->RefreshOptions();
		BranchCombo->SetSelectedItem(SelectedBranch);
	}
}

void SSteamPublishPanel::RefreshHistory()
{
	HistoryEntries.Reset();
	for (const FSteamPublishHistoryEntry& Entry : FSteamPublishHistory::Load())
	{
		HistoryEntries.Add(MakeShared<FSteamPublishHistoryEntry>(Entry));
	}
	if (HistoryList.IsValid())
	{
		HistoryList->RequestListRefresh();
	}
}

void SSteamPublishPanel::RebuildChecks()
{
	ChecksBox->ClearChildren();
	for (const FSteamValidationCheck& Check : Checks)
	{
		ChecksBox->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(4.f, 4.f, 8.f, 0.f)
			[
				SNew(SBox).WidthOverride(10.f).HeightOverride(10.f)
				[
					SNew(SImage).Image(FSteamToolStyle::Get().GetBrush(GetSeverityBrush(Check.Severity)))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Check.Label).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Check.Detail).AutoWrapText(true).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Visibility(Check.Fix ? EVisibility::Visible : EVisibility::Collapsed)
				.OnClicked_Lambda([this, Fix = Check.Fix]()
				{
					Fix();
					StatusText = LOCTEXT("FixApplied", "Fix applied. Press Publish again to re-check.");
					StatusSeverity = ESteamCheckSeverity::Info;
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Check.FixLabel).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
				]
			]
		];
	}
}

bool SSteamPublishPanel::IsBranchSetLive(const FString& Branch) const
{
	if (Branch.IsEmpty() || !FSteamVdfWriter::CanSetLive(Branch))
	{
		return false;
	}
	for (const FSteamPublishBranch& Entry : USteamPublishSettings::Get()->Branches)
	{
		if (Entry.Name.TrimStartAndEnd() == Branch)
		{
			return Entry.bSetLive;
		}
	}
	return false;
}

void SSteamPublishPanel::OnPublishClicked(bool bDryRun, bool bSkipUpload)
{
	if (IsRunning())
	{
		return;
	}

	FSteamPublishOptions Options;
	Options.Branch = SelectedBranch.IsValid() ? *SelectedBranch : FString();
	Options.bDryRun = bDryRun;
	Options.bSkipPackaging = bSkipPackaging;
	Options.bSkipUpload = bSkipUpload;
	Options.bDisableLiveCoding = bDisableLiveCoding;

	Checks = FSteamPublishPreflight::Run(Options);
	RebuildChecks();
	if (FSteamPublishPreflight::HasErrors(Checks))
	{
		StatusText = LOCTEXT("PreflightFailed", "Fix the problems above, then try again.");
		StatusSeverity = ESteamCheckSeverity::Error;
		return;
	}

	if (!ConfirmRun(Options))
	{
		StatusText = LOCTEXT("RunCancelled", "Cancelled. Nothing was started.");
		StatusSeverity = ESteamCheckSeverity::Info;
		return;
	}

	StartJob(Options);
}

bool SSteamPublishPanel::ConfirmRun(const FSteamPublishOptions& Options) const
{
	const USteamPublishSettings* Settings = USteamPublishSettings::Get();
	const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
	const bool bUpload = !Options.bDryRun && !Options.bSkipUpload;
	const bool bPackage = !Options.bDryRun && !Options.bSkipPackaging;
	const bool bLive = bUpload && IsBranchSetLive(Options.Branch);
	const FText BranchText = Options.Branch.IsEmpty() ? LOCTEXT("NoBranchValue", "(none)") : FText::FromString(Options.Branch);
	const FText ConfigText = StaticEnum<ESteamPublishConfig>()->GetDisplayNameTextByValue(static_cast<int64>(Settings->PackageConfig));
	const FText Runs = LOCTEXT("StepRuns", "Runs");
	const FText Skipped = LOCTEXT("StepSkipped", "Skipped");

	FSteamConfirmRequest Request;
	Request.Icon = FSteamToolStyle::Get().GetBrush("SandwichSteam.Publish40");
	Request.Footer = LOCTEXT("ConfirmFooter", "The run can take several minutes. Cancel it at any time from the Publish page; the full log is written to the publish directory.");

	// Mode: title, what it does, the one consequence that matters most.
	if (Options.bDryRun)
	{
		Request.Title = LOCTEXT("ConfirmDryTitle", "Confirm dry run");
		Request.Intro = LOCTEXT("ConfirmDryIntro", "Writes the app and depot build scripts from your settings so you can inspect them. Nothing is packaged and nothing is uploaded.");
		Request.BannerMark = ESteamConfirmMark::Ok;
		Request.BannerText = LOCTEXT("ConfirmDryBanner", "Safe: only build script files are written.");
		Request.ConfirmLabel = LOCTEXT("ConfirmDryButton", "Write build scripts");
	}
	else if (Options.bSkipUpload)
	{
		Request.Title = LOCTEXT("ConfirmTestTitle", "Confirm test build");
		Request.Intro = LOCTEXT("ConfirmTestIntro", "Runs the whole pipeline except the upload, so you can check that packaging works before spending an upload.");
		Request.BannerMark = ESteamConfirmMark::Ok;
		Request.BannerText = LOCTEXT("ConfirmTestBanner", "Nothing is uploaded to Steam.");
		Request.ConfirmLabel = LOCTEXT("ConfirmTestButton", "Start test build");
	}
	else
	{
		Request.Title = LOCTEXT("ConfirmPublishTitle", "Confirm publish");
		Request.Intro = LOCTEXT("ConfirmPublishIntro", "Packages the game and uploads a new build to Steam with SteamCMD. Check the settings below once more.");
		if (bLive)
		{
			Request.BannerMark = ESteamConfirmMark::Warning;
			Request.BannerText = FText::Format(LOCTEXT("ConfirmLiveBanner", "Goes live on branch '{0}' as soon as the upload finishes."), BranchText);
			Request.ConfirmLabel = LOCTEXT("ConfirmLiveButton", "Publish and set live");
			FSteamConfirmOption& Acknowledge = Request.Options.AddDefaulted_GetRef();
			Acknowledge.Id = TEXT("AcknowledgeLive");
			Acknowledge.Label = FText::Format(LOCTEXT("AcknowledgeLive", "I understand players on '{0}' get this build right away."), BranchText);
			Acknowledge.bRequired = true;
		}
		else
		{
			Request.BannerMark = ESteamConfirmMark::Info;
			Request.BannerText = LOCTEXT("ConfirmNotLiveBanner", "Uploads a new build but does not set it live. Set it live later on the partner site (SteamPipe > Builds).");
			Request.ConfirmLabel = LOCTEXT("ConfirmPublishButton", "Publish");
		}
	}

	// What will happen, in order, with the steps this mode skips greyed out.
	{
		FSteamConfirmSection& Steps = Request.AddSection(LOCTEXT("SectionSteps", "What will happen"));
		const bool bPre = !Options.bDryRun && !Settings->PreSteps.IsEmpty();
		const bool bPost = !Options.bDryRun && !Settings->PostSteps.IsEmpty();

		Steps.AddRow(FSteamPublishJob::GetStepLabel(ESteamPublishStepId::PreSteps), bPre ? Runs : Skipped,
			bPre ? FText::Format(LOCTEXT("PreNote", "{0} command(s) from the Build settings."), Settings->PreSteps.Num())
				: (Options.bDryRun ? LOCTEXT("PreDryNote", "Not part of a dry run.") : LOCTEXT("PreNoneNote", "No pre steps are set up.")),
			bPre ? ESteamConfirmMark::Info : ESteamConfirmMark::Idle);

		FText PackageNote = LOCTEXT("PackageDryNote", "Not part of a dry run.");
		if (Options.bSkipPackaging && !Options.bDryRun)
		{
			PackageNote = LOCTEXT("PackageSkipNote", "\"Upload staged build\" is checked: the folder that is already staged is used as it is.");
		}
		else if (bPackage)
		{
			PackageNote = FText::Format(LOCTEXT("PackageNote", "UAT BuildCookRun, {0}. Live Coding is {1} while packaging."), ConfigText,
				Options.bDisableLiveCoding ? LOCTEXT("LiveCodingPaused", "paused") : LOCTEXT("LiveCodingKept", "left as it is"));
		}
		Steps.AddRow(FSteamPublishJob::GetStepLabel(ESteamPublishStepId::Package), bPackage ? Runs : Skipped, PackageNote,
			bPackage ? ESteamConfirmMark::Info : ESteamConfirmMark::Idle);

		Steps.AddRow(FSteamPublishJob::GetStepLabel(ESteamPublishStepId::Vdf), Runs,
			FText::Format(LOCTEXT("VdfNote", "app_build and depot_build scripts in {0}"), FText::FromString(SandwichSteam::Publish::GetPublishDir())),
			ESteamConfirmMark::Info);

		FText UploadNote = Options.bDryRun ? LOCTEXT("UploadDryNote", "Not part of a dry run.") : LOCTEXT("UploadTestNote", "Not part of a test build.");
		if (bUpload)
		{
			UploadNote = FText::Format(LOCTEXT("UploadNote", "SteamCMD as {0}, cached login (no password is sent)."), FText::FromString(User->SteamUsername.TrimStartAndEnd()));
		}
		Steps.AddRow(FSteamPublishJob::GetStepLabel(ESteamPublishStepId::Upload), bUpload ? Runs : Skipped, UploadNote,
			bUpload ? (bLive ? ESteamConfirmMark::Warning : ESteamConfirmMark::Info) : ESteamConfirmMark::Idle);

		Steps.AddRow(FSteamPublishJob::GetStepLabel(ESteamPublishStepId::PostSteps), bPost ? Runs : Skipped,
			bPost ? FText::Format(LOCTEXT("PostNote", "{0} command(s), after a successful run."), Settings->PostSteps.Num())
				: (Options.bDryRun ? LOCTEXT("PostDryNote", "Not part of a dry run.") : LOCTEXT("PostNoneNote", "No post steps are set up.")),
			bPost ? ESteamConfirmMark::Info : ESteamConfirmMark::Idle);
	}

	// The settings this run uses.
	{
		FSteamConfirmSection& Build = Request.AddSection(LOCTEXT("SectionBuild", "Build"));
		Build.AddRow(LOCTEXT("RowAppId", "Steam App ID"), FText::AsNumber(Settings->GetAppId(), &FNumberFormattingOptions::DefaultNoGrouping()));
		Build.AddRow(LOCTEXT("RowBranch", "Branch"), BranchText,
			Options.Branch.IsEmpty() ? LOCTEXT("BranchNoneNote", "The build is uploaded without being set live anywhere.")
				: (IsBranchSetLive(Options.Branch) ? LOCTEXT("BranchLiveNote", "Set live is on for this branch.") : LOCTEXT("BranchNotLiveNote", "Set live is off for this branch (or it is 'default', which Steam never sets live from SteamCMD).")));
		Build.AddRow(LOCTEXT("RowConfig", "Configuration"), ConfigText);
		Build.AddRow(LOCTEXT("RowTarget", "Target"), Settings->TargetName.IsEmpty() ? LOCTEXT("TargetDefault", "Project default") : FText::FromString(Settings->TargetName));
		Build.AddRow(LOCTEXT("RowDescription", "Build description"), FText::FromString(SandwichSteam::Publish::FormatBuildDescription(*Settings, Options.Branch)),
			FText::Format(LOCTEXT("DescriptionNote", "From the template \"{0}\". Shown on the partner site's builds page."), FText::FromString(Settings->BuildDescriptionTemplate)));
		if (bUpload)
		{
			Build.AddRow(LOCTEXT("RowAccount", "Steam account"), FText::FromString(User->SteamUsername.TrimStartAndEnd()));
		}
		Build.AddRow(LOCTEXT("RowStaging", "Staging directory"), FText::FromString(SandwichSteam::Publish::GetStagingDir()));
	}

	// One row per enabled depot: what is uploaded from where.
	{
		FSteamConfirmSection& Depots = Request.AddSection(LOCTEXT("SectionDepots", "Depots"));
		for (const FSteamPublishDepot& Depot : Settings->Depots)
		{
			if (!Depot.bEnabled)
			{
				continue;
			}
			const FString Root = SandwichSteam::Publish::ResolveContentRoot(Depot);
			const bool bExists = IFileManager::Get().DirectoryExists(*Root);

			FText Note = LOCTEXT("DepotReady", "Content folder found.");
			ESteamConfirmMark Mark = ESteamConfirmMark::Ok;
			if (!bExists)
			{
				// Packaging creates the folder; without it the upload has nothing to send.
				Note = bPackage ? LOCTEXT("DepotCreated", "Created by packaging.") : LOCTEXT("DepotMissing", "The content folder does not exist yet. Package first.");
				Mark = bPackage ? ESteamConfirmMark::Info : ESteamConfirmMark::Warning;
			}
			Depots.AddRow(FText::Format(LOCTEXT("DepotLabel", "{0} ({1})"), FText::AsNumber(Depot.DepotId, &FNumberFormattingOptions::DefaultNoGrouping()),
					StaticEnum<ESteamPublishPlatform>()->GetDisplayNameTextByValue(static_cast<int64>(Depot.Platform))),
				FText::FromString(Root), Note, Mark);
		}
	}

	// Warnings from the pre-flight checks that just ran (errors never get here: the run stops before the dialog).
	Request.Sections.Add(SandwichSteam::Editor::MakeChecksSection(LOCTEXT("SectionChecks", "Pre-flight checks"), Checks, /*bIssuesOnly*/ true));

	return SandwichSteam::Editor::ShowConfirmDialog(Request).bConfirmed;
}

void SSteamPublishPanel::StartJob(const FSteamPublishOptions& Options)
{
	LogLines.Reset();
	bLogDirty = true;
	for (TPair<ESteamPublishStepId, ESteamPublishStepState>& Pair : StepStates)
	{
		Pair.Value = ESteamPublishStepState::Pending;
	}
	ProgressValue = -1.f;

	Job = MakeShared<FSteamPublishJob>();
	Job->OnLog().AddSP(this, &SSteamPublishPanel::HandleLog);
	Job->OnStepChanged().AddSP(this, &SSteamPublishPanel::HandleStepChanged);
	Job->OnProgress().AddSP(this, &SSteamPublishPanel::HandleProgress);
	Job->OnStatus().AddSP(this, &SSteamPublishPanel::HandleStatus);
	Job->OnGuardRequested().AddSP(this, &SSteamPublishPanel::HandleGuardRequested);
	Job->OnFinished().AddSP(this, &SSteamPublishPanel::HandleFinished);

	StatusSeverity = ESteamCheckSeverity::Info;
	FString Error;
	if (!Job->Start(Options, Error))
	{
		StatusText = FText::FromString(Error);
		StatusSeverity = ESteamCheckSeverity::Error;
		Job.Reset();
	}
}

void SSteamPublishPanel::HandleLog(const FString& Line, ESteamLogSeverity Severity)
{
	TSharedPtr<FLogLine> Entry = MakeShared<FLogLine>();
	Entry->Text = Line;
	Entry->Severity = Severity;
	LogLines.Add(Entry);
	if (LogLines.Num() > MaxLogLines)
	{
		LogLines.RemoveAt(0, 500, EAllowShrinking::No);
	}
	bLogDirty = true;
}

void SSteamPublishPanel::HandleStepChanged(ESteamPublishStepId Step, ESteamPublishStepState State)
{
	StepStates.Add(Step, State);
}

void SSteamPublishPanel::HandleProgress(float Progress)
{
	ProgressValue = Progress;
}

void SSteamPublishPanel::HandleStatus(const FText& Status)
{
	StatusText = Status;
	StatusSeverity = ESteamCheckSeverity::Info;
}

void SSteamPublishPanel::HandleGuardRequested(bool bMobile)
{
	// The dialog is modal: open it on the next tick, outside the event delivery.
	const TWeakPtr<SSteamPublishPanel> WeakPanel = StaticCastSharedRef<SSteamPublishPanel>(AsShared());
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPanel, bMobile](float)
	{
		if (const TSharedPtr<SSteamPublishPanel> Panel = WeakPanel.Pin())
		{
			if (Panel->IsRunning())
			{
				const TOptional<FString> Code = SandwichSteam::Editor::PromptSteamGuardCode(bMobile);
				if (Code.IsSet())
				{
					Panel->Job->SubmitGuardCode(Code.GetValue());
				}
				else
				{
					Panel->Job->Cancel();
				}
			}
		}
		return false;
	}));
}

void SSteamPublishPanel::HandleFinished(const FSteamPublishResult& Result)
{
	ProgressValue = Result.bSuccess ? 1.f : 0.f;
	StatusText = FText::FromString(Result.Message);
	StatusSeverity = Result.bSuccess ? ESteamCheckSeverity::Ok : (Result.bCanceled ? ESteamCheckSeverity::Warning : ESteamCheckSeverity::Error);

	Notify(FText::FromString(Result.Message), Result.bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	RefreshHistory();
}

void SSteamPublishPanel::HandleObjectChanged(UObject* Object, FPropertyChangedEvent& /*Event*/)
{
	if (Object == GetDefault<USteamPublishSettings>())
	{
		RefreshBranches();
	}
}

namespace SandwichSteam::Editor
{
	void RegisterPublishTab()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PublishTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			const TSharedRef<SSteamPublishPanel> Panel = SNew(SSteamPublishPanel);
			const TSharedRef<SDockTab> Tab = SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("TabLabel", "Steam Publish"))
				.OnCanCloseTab_Lambda([WeakPanel = TWeakPtr<SSteamPublishPanel>(Panel)]()
				{
					const TSharedPtr<SSteamPublishPanel> Pinned = WeakPanel.Pin();
					return Pinned.IsValid() ? Pinned->CanCloseTab() : true;
				})
				[
					Panel
				];
			return Tab;
		}))
		.SetDisplayName(LOCTEXT("TabDisplayName", "Steam Publish"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Package the game and upload it to Steam."))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Publish16"));
	}

	void UnregisterPublishTab()
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PublishTabName);
		}
	}

	void OpenPublishTab()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(PublishTabName);
	}

	void RegisterPublishDashboardPage()
	{
		const TSharedRef<TWeakPtr<SSteamPublishPanel>> PanelRef = MakeShared<TWeakPtr<SSteamPublishPanel>>();

		FSteamDashboardPage Page;
		Page.Id = TEXT("Publish");
		Page.Label = LOCTEXT("PublishPageLabel", "Publish");
		Page.Icon = FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Publish16");
		Page.Order = 30;
		Page.BuildContent = [PanelRef]() -> TSharedRef<SWidget>
		{
			const TSharedRef<SSteamPublishPanel> Panel = SNew(SSteamPublishPanel);
			*PanelRef = Panel;
			return Panel;
		};
		Page.CanClose = [PanelRef]()
		{
			const TSharedPtr<SSteamPublishPanel> Panel = PanelRef->Pin();
			return !Panel.IsValid() || Panel->CanCloseTab();
		};

		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
