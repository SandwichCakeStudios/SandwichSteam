// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Setup/SSteamSetupPage.h"
#include "Cook/SteamCookHelper.h"
#include "Core/SteamToolSettings.h"
#include "Dashboard/SSteamDashboardPanel.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Publish/SteamCmdOutputParser.h"
#include "Publish/SteamCmdSetupService.h"
#include "Publish/SteamPublishSettings.h"
#include "Settings/SteamConfigureAction.h"
#include "Setup/SteamSetupSteps.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "UObject/UObjectGlobals.h"
#include "Validation/SteamProjectValidator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDashboardSetup"

namespace
{
	constexpr float StatusIconSize = 18.f;
	constexpr float BlockedOpacity = 0.55f;

	FSlateFontInfo BodyFont() { return FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f); }
	FSlateFontInfo HintFont() { return FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.1f); }
	FSlateFontInfo TitleFont() { return FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.4f); }

	void Notify(const FText& Text, SNotificationItem::ECompletionState State)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 8.0f;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	/** Opens Project Settings (or Editor Preferences) on the section of these settings. */
	void OpenSettings(const UDeveloperSettings* Settings)
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer(Settings->GetContainerName(), Settings->GetCategoryName(), Settings->GetSectionName());
		}
	}

	/** Sets one property the way the details panel would, so every listener (status panel, Message Log, open details views) sees the edit. */
	void EditSettingsProperty(UObject* Object, FName PropertyName, TFunctionRef<void()> Assign)
	{
		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), PropertyName);
		Object->PreEditChange(Property);
		Assign();
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Object->PostEditChangeProperty(Event);
	}

	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick, bool bPrimary, TAttribute<bool> bEnabled = true)
	{
		return SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(bPrimary ? "PrimaryButton" : "Button"))
			.ToolTipText(ToolTip)
			.IsEnabled(bEnabled)
			.ContentPadding(FMargin(12.f, 4.f))
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(BodyFont())
			];
	}

	/** Small subdued line under a step's buttons, with an optional link at the end. */
	TSharedRef<SWidget> MakeHint(const FText& Text, const FText& LinkText = FText::GetEmpty(), TFunction<void()> OnLink = nullptr)
	{
		const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Text).Font(HintFont()).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];

		if (OnLink)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SHyperlink).Text(LinkText).OnNavigate_Lambda([OnLink]() { OnLink(); })
			];
		}
		return Row;
	}

	FText GetStepTitle(ESteamSetupStepId Id)
	{
		switch (Id)
		{
		case ESteamSetupStepId::AppId:			return LOCTEXT("TitleAppId", "Steam App ID");
		case ESteamSetupStepId::AppDefinition:	return LOCTEXT("TitleDefinition", "App Definition");
		case ESteamSetupStepId::SteamCmd:		return LOCTEXT("TitleSteamCmd", "SteamCMD");
		case ESteamSetupStepId::Account:		return LOCTEXT("TitleAccount", "Steam account");
		case ESteamSetupStepId::Depots:			return LOCTEXT("TitleDepots", "Depots & branches");
		default:								return FText::GetEmpty();
		}
	}

	/**
	 * The Setup page. State is cached and rebuilt only when something it reads changes (settings edits, the App Definition being
	 * reassigned, a SteamCMD job starting or finishing), on the next tick: same idiom as SSteamStatusPanel. Progress bars read
	 * live values through lambdas, so a download does not rebuild the page.
	 */
	class SSteamSetupPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSteamSetupPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& /*InArgs*/)
		{
			ChildSlot
			[
				SNew(SBox).Padding(4.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(Content, SVerticalBox)
					]
				]
			];

			ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SSteamSetupPage::HandleObjectChanged);
			DefinitionChangedHandle = USteamToolSettings::OnAppDefinitionChanged().AddSP(this, &SSteamSetupPage::RequestRefresh);
			FSteamCmdSetupService::Get().OnChanged().AddSP(this, &SSteamSetupPage::RequestRefresh);

			Refresh();
		}

		virtual ~SSteamSetupPage() override
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
			USteamToolSettings::OnAppDefinitionChanged().Remove(DefinitionChangedHandle);
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
			if (bRefreshRequested)
			{
				Refresh();
			}
		}

		/** Shown while setup is incomplete, and kept for the life of the tab once shown, so it never vanishes under the user. */
		bool ShouldShowInNav() const { return bShowInNav; }

	private:
		void HandleObjectChanged(UObject* Object, FPropertyChangedEvent& /*Event*/)
		{
			if (Object == GetDefault<USteamToolSettings>() || Object == GetDefault<USteamPublishSettings>() || Object == GetDefault<USteamPublishUserSettings>())
			{
				RequestRefresh();
			}
		}

		void RequestRefresh()
		{
			bRefreshRequested = true;
		}

		void Refresh()
		{
			bRefreshRequested = false;

			FSteamCmdSetupService::Get().AdoptDownloadedSteamCmd();

			Inputs = SandwichSteam::Editor::GatherSetupInputs();
			Steps = SandwichSteam::Editor::EvaluateSetupSteps(Inputs);
			const bool bComplete = SandwichSteam::Editor::IsSetupComplete(Steps);
			bShowInNav |= !bComplete;

			RunAutomaticSteps();
			Rebuild(bComplete);
		}

		/**
		 * Does what needs no decision from the user, once per tab: checks the cached login, then fetches the depots. Only while
		 * setup is incomplete, so a finished project never starts SteamCMD just because the dashboard opened.
		 */
		void RunAutomaticSteps()
		{
			FSteamCmdSetupService& Service = FSteamCmdSetupService::Get();
			if (!bShowInNav || Service.IsAnyRunning() || GetStep(ESteamSetupStepId::Depots).State == ESteamSetupStepState::Done)
			{
				return;
			}

			FString Error;
			if (!bAutoLoginTried && Inputs.bSteamCmdFound && Inputs.bUsernameValid && Inputs.LoginState == ESteamSetupLoginState::Unknown)
			{
				bAutoLoginTried = true;
				Service.StartLoginCheck(/*bInteractive*/ false, Error);
				return;
			}
			if (!bAutoFetchTried && GetStep(ESteamSetupStepId::Depots).State == ESteamSetupStepState::Todo)
			{
				bAutoFetchTried = true;
				Service.StartAppInfoFetch(ESteamCmdTaskMode::Automatic, Error);
			}
		}

		const FSteamSetupStep& GetStep(ESteamSetupStepId Id) const
		{
			return Steps[static_cast<int32>(Id)];
		}

		void Rebuild(bool bComplete)
		{
			Content->ClearChildren();

			int32 DoneCount = 0;
			for (const FSteamSetupStep& Step : Steps)
			{
				DoneCount += Step.State == ESteamSetupStepState::Done ? 1 : 0;
			}

			// Header: title, one line of intro, "n of 5 done" with a bar.
			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock).Text(LOCTEXT("PageTitle", "Setup")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
			];
			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Font(BodyFont()).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LOCTEXT("Intro", "Everything this project needs to publish to Steam, in order. Each button does the work for you."))
			];
			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(STextBlock).Font(BodyFont()).Text(FText::Format(LOCTEXT("Progress", "{0} of {1} done"), DoneCount, Steps.Num()))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(6.f)
					[
						SNew(SProgressBar).Percent(Steps.Num() > 0 ? static_cast<float>(DoneCount) / Steps.Num() : 0.f)
					]
				]
			];

			if (bComplete)
			{
				Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
				[
					BuildAllSetCard()
				];
			}

			const int32 NextIndex = SandwichSteam::Editor::FindNextSetupStep(Steps);
			for (int32 Index = 0; Index < Steps.Num(); ++Index)
			{
				Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					BuildStepCard(Index, Steps[Index], Index == NextIndex)
				];
			}
		}

		TSharedRef<SWidget> BuildAllSetCard()
		{
			return SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(14.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f, 12.f, 0.f)
				[
					SNew(SBox).WidthOverride(24.f).HeightOverride(24.f)
					[
						SNew(SImage).Image(FSteamToolStyle::Get().GetBrush("SandwichSteam.Status.Ok"))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(LOCTEXT("AllSetTitle", "You're all set")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.6f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 10.f)
					[
						SNew(STextBlock).Font(BodyFont()).AutoWrapText(true)
						.Text(LOCTEXT("AllSetBody", "Steam is set up for this project. Publish a build whenever you are ready. This page hides itself the next time the dashboard opens."))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
						[
							MakeButton(LOCTEXT("OpenPublish", "Open Publish"), LOCTEXT("OpenPublishTip", "Goes to the Publish page."),
								[]() { SandwichSteam::Editor::OpenDashboardPage(TEXT("Publish")); }, /*bPrimary*/ true)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeButton(LOCTEXT("Validate", "Validate project"), LOCTEXT("ValidateTip", "Runs every Sandwich Steam check and opens the Message Log when something needs attention."),
								[]() { FSteamProjectValidator::RunAndLog(/*bOpen*/ true); }, /*bPrimary*/ false)
						]
					]
				]
			];
		}

		TSharedRef<SWidget> BuildStatusIcon(ESteamSetupStepState State)
		{
			if (State == ESteamSetupStepState::Running)
			{
				return SNew(SCircularThrobber).Radius(StatusIconSize * 0.5f);
			}

			const TCHAR* Brush = TEXT("SandwichSteam.Status.Warning");
			if (State == ESteamSetupStepState::Done)
			{
				Brush = TEXT("SandwichSteam.Status.Ok");
			}
			else if (State == ESteamSetupStepState::Blocked)
			{
				Brush = TEXT("SandwichSteam.Status.Info");
			}
			return SNew(SImage).Image(FSteamToolStyle::Get().GetBrush(Brush));
		}

		TSharedRef<SWidget> BuildStepCard(int32 Index, const FSteamSetupStep& Step, bool bIsNext)
		{
			const TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Font(TitleFont()).Text(FText::Format(LOCTEXT("StepTitle", "{0}.  {1}"), Index + 1, GetStepTitle(Step.Id)))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Visibility(bIsNext ? EVisibility::Visible : EVisibility::Collapsed)
						.Text(LOCTEXT("NextTag", "NEXT"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(FStyleColors::Primary)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(BodyFont()).AutoWrapText(true).Text(Step.Detail)
					.ColorAndOpacity(Step.State == ESteamSetupStepState::Done ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
				];

			if (Step.State != ESteamSetupStepState::Done && Step.State != ESteamSetupStepState::Blocked)
			{
				Body->AddSlot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
				[
					BuildStepActions(Step, bIsNext)
				];
			}

			return SNew(SBorder)
				.BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card"))
				.Padding(12.f)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Step.State == ESteamSetupStepState::Blocked ? BlockedOpacity : 1.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f, 2.f, 12.f, 0.f)
					[
						SNew(SBox).WidthOverride(StatusIconSize).HeightOverride(StatusIconSize)
						[
							BuildStatusIcon(Step.State)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						Body
					]
				];
		}

		TSharedRef<SWidget> BuildStepActions(const FSteamSetupStep& Step, bool bIsNext)
		{
			switch (Step.Id)
			{
			case ESteamSetupStepId::AppId:			return BuildAppIdActions(bIsNext);
			case ESteamSetupStepId::AppDefinition:	return BuildDefinitionActions(bIsNext);
			case ESteamSetupStepId::SteamCmd:		return BuildSteamCmdActions(Step, bIsNext);
			case ESteamSetupStepId::Account:		return BuildAccountActions(Step, bIsNext);
			case ESteamSetupStepId::Depots:			return BuildDepotActions(Step, bIsNext);
			default:								return SNullWidget::NullWidget;
			}
		}

		//~ Step 1: App ID

		int32 GetShownAppId() const
		{
			return PendingAppId.IsSet() ? PendingAppId.GetValue() : Inputs.AppId;
		}

		bool CanApplyAppId() const
		{
			const int32 AppId = GetShownAppId();
			return AppId > 0 && AppId != SandwichSteam::Editor::SpacewarAppId;
		}

		void ApplyAppId()
		{
			if (!CanApplyAppId())
			{
				return;
			}
			const int32 AppId = GetShownAppId();
			PendingAppId.Reset();

			USteamToolSettings* Settings = GetMutableDefault<USteamToolSettings>();
			EditSettingsProperty(Settings, GET_MEMBER_NAME_CHECKED(USteamToolSettings, SteamAppId), [Settings, AppId]() { Settings->SteamAppId = AppId; });
			Settings->TryUpdateDefaultConfigFile();

			// SteamDevAppId in DefaultEngine.ini follows the App ID; this shows the change and asks before writing.
			SandwichSteam::Editor::ConfigureSteam();
			RequestRefresh();
		}

		TSharedRef<SWidget> BuildAppIdActions(bool bIsNext)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SBox).WidthOverride(160.f)
						[
							SNew(SNumericEntryBox<int32>)
							.AllowSpin(false)
							.MinValue(0)
							.Font(BodyFont())
							.Value_Lambda([this]() { return TOptional<int32>(GetShownAppId()); })
							.OnValueChanged_Lambda([this](int32 Value) { PendingAppId = Value; })
							.OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type CommitType)
							{
								PendingAppId = Value;
								if (CommitType == ETextCommit::OnEnter)
								{
									ApplyAppId();
								}
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						MakeButton(LOCTEXT("SaveAppId", "Save App ID"),
							LOCTEXT("SaveAppIdTip", "Saves the App ID in the Sandwich Steam settings, then shows the matching DefaultEngine.ini change (SteamDevAppId) and applies it."),
							[this]() { ApplyAppId(); }, bIsNext, TAttribute<bool>::CreateLambda([this]() { return CanApplyAppId(); }))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					MakeHint(LOCTEXT("AppIdHint", "You can also set it yourself in Project Settings > Plugins > Sandwich Steam."),
						LOCTEXT("OpenSettings", "Open settings"), []() { OpenSettings(GetDefault<USteamToolSettings>()); })
				];
		}

		//~ Step 2: App Definition

		TSharedRef<SWidget> BuildDefinitionActions(bool bIsNext)
		{
			const bool bOnlyCook = Inputs.bDefinitionLoads && !Inputs.bDefinitionCooked;
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					bOnlyCook
					? MakeButton(LOCTEXT("AddToCook", "Add to cook"), LOCTEXT("AddToCookTip", "Makes sure packaged builds include the assigned App Definition."),
						[this]() { FSteamCookHelper::EnsureAppDefinitionCooked(); RequestRefresh(); }, bIsNext)
					: MakeButton(LOCTEXT("CreateDefinition", "Create & assign"),
						LOCTEXT("CreateDefinitionTip", "Creates /Game/Steam/DA_SteamAppDefinition, saves it, assigns it in the Sandwich Steam settings and adds it to the cook."),
						[this]()
						{
							FSteamProjectValidator::CreateAndAssignAppDefinition();
							FSteamCookHelper::EnsureAppDefinitionCooked();
							RequestRefresh();
						}, bIsNext)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					MakeHint(LOCTEXT("DefinitionHint", "Already have one? Assign it yourself under App Definition in Project Settings > Plugins > Sandwich Steam."),
						LOCTEXT("OpenSettings", "Open settings"), []() { OpenSettings(GetDefault<USteamToolSettings>()); })
				];
		}

		//~ Step 3: SteamCMD

		TSharedRef<SWidget> BuildSteamCmdActions(const FSteamSetupStep& Step, bool bIsNext)
		{
			if (Step.State == ESteamSetupStepState::Running)
			{
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(6.f)
						[
							SNew(SProgressBar).Percent_Lambda([]() -> TOptional<float>
							{
								const float Progress = FSteamCmdSetupService::Get().GetDownloadProgress();
								return Progress >= 0.f ? TOptional<float>(Progress) : TOptional<float>();
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(HintFont()).ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Text_Lambda([]() { return FSteamCmdSetupService::Get().GetDownloadStep(); })
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeButton(LOCTEXT("Cancel", "Cancel"), LOCTEXT("CancelDownloadTip", "Stops the download or extraction."),
								[]() { FSteamCmdSetupService::Get().CancelDownload(); }, /*bPrimary*/ false)
						]
					];
			}

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeButton(LOCTEXT("InstallSteamCmd", "Install SteamCMD"),
						LOCTEXT("InstallSteamCmdTip", "Downloads Valve's official SteamCMD for this platform into <Data Directory>/SteamCMD, extracts and updates it, and sets the SteamCMD path for you."),
						[]() { SandwichSteam::Editor::ConfirmAndDownloadSteamCmd(ESteamCmdTaskMode::Automatic); }, bIsNext)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					MakeHint(LOCTEXT("SteamCmdHint", "Already have SteamCMD? Set its path yourself on the SteamCMD page."),
						LOCTEXT("OpenSteamCmdPage", "Open SteamCMD page"), []() { SandwichSteam::Editor::OpenDashboardPage(TEXT("SteamCmd")); })
				];
		}

		//~ Step 4: Steam account

		FString GetShownUsername() const
		{
			return PendingUsername.IsSet() ? PendingUsername.GetValue() : USteamPublishUserSettings::Get()->SteamUsername;
		}

		/** Saves the name typed in the box (if it changed). */
		void ApplyUsername()
		{
			if (!PendingUsername.IsSet())
			{
				return;
			}
			const FString Username = PendingUsername.GetValue().TrimStartAndEnd();
			PendingUsername.Reset();

			USteamPublishUserSettings* User = GetMutableDefault<USteamPublishUserSettings>();
			if (Username == User->SteamUsername)
			{
				return;
			}
			EditSettingsProperty(User, GET_MEMBER_NAME_CHECKED(USteamPublishUserSettings, SteamUsername), [User, &Username]() { User->SteamUsername = Username; });
			User->SaveConfig();
			RequestRefresh();
		}

		bool IsShownUsernameValid() const
		{
			return FSteamCmdCommandLine::IsValidUsername(GetShownUsername().TrimStartAndEnd());
		}

		TSharedRef<SWidget> BuildAccountActions(const FSteamSetupStep& Step, bool bIsNext)
		{
			if (Step.State == ESteamSetupStepState::Running)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left)
					[
						MakeButton(LOCTEXT("Cancel", "Cancel"), LOCTEXT("CancelLoginTip", "Stops the login check."),
							[]() { FSteamCmdSetupService::Get().CancelLoginCheck(); }, /*bPrimary*/ false)
					];
			}

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SBox).WidthOverride(220.f)
						[
							SNew(SEditableTextBox)
							.Font(BodyFont())
							.HintText(LOCTEXT("UsernameHint", "Steam account name"))
							.Text_Lambda([this]() { return FText::FromString(GetShownUsername()); })
							.OnTextChanged_Lambda([this](const FText& Text) { PendingUsername = Text.ToString(); })
							.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
							{
								PendingUsername = Text.ToString();
								ApplyUsername();
							})
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
					[
						MakeButton(LOCTEXT("LogIn", "Log in..."),
							LOCTEXT("LogInTip", "Opens a terminal running SteamCMD. Type your password and Steam Guard code there once; SteamCMD remembers the login. It is checked automatically when you come back to the editor."),
							[this]()
							{
								ApplyUsername();
								FSteamCmdSetupService::Get().BeginTerminalLogin();
							}, bIsNext, TAttribute<bool>::CreateLambda([this]() { return IsShownUsernameValid(); }))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						MakeButton(LOCTEXT("Verify", "Verify"),
							LOCTEXT("VerifyTip", "Checks the login SteamCMD remembers (steamcmd +login <account> +quit). A Steam Guard prompt opens a dialog."),
							[this]()
							{
								ApplyUsername();
								FString Error;
								if (!FSteamCmdSetupService::Get().StartLoginCheck(/*bInteractive*/ true, Error))
								{
									Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
								}
							}, /*bPrimary*/ false, TAttribute<bool>::CreateLambda([this]() { return IsShownUsernameValid(); }))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					MakeHint(LOCTEXT("AccountHint", "Your password is typed only into SteamCMD's own terminal; this plugin never sees or stores it. The account can also be set on the SteamCMD page."),
						LOCTEXT("OpenSteamCmdPage", "Open SteamCMD page"), []() { SandwichSteam::Editor::OpenDashboardPage(TEXT("SteamCmd")); })
				];
		}

		//~ Step 5: Depots & branches

		TSharedRef<SWidget> BuildDepotActions(const FSteamSetupStep& Step, bool bIsNext)
		{
			const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

			if (Step.State == ESteamSetupStepState::Running)
			{
				Box->AddSlot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeButton(LOCTEXT("Cancel", "Cancel"), LOCTEXT("CancelFetchTip", "Stops reading the app info."),
						[]() { FSteamCmdSetupService::Get().CancelAppInfoFetch(); }, /*bPrimary*/ false)
				];
				return Box;
			}

			Box->AddSlot().AutoHeight().HAlign(HAlign_Left)
			[
				MakeButton(LOCTEXT("FetchDepots", "Fetch from Steam"),
					LOCTEXT("FetchDepotsTip", "Reads your app's depots and branches with SteamCMD (app_info_print) and adds them to Project Settings > Plugins > Sandwich Steam - Publish. Existing entries are kept."),
					[]() { SandwichSteam::Editor::ConfirmAndFetchAppInfo(ESteamCmdTaskMode::Automatic); }, bIsNext)
			];

			const FText& Summary = FSteamCmdSetupService::Get().GetLastAppInfoSummary();
			if (!Summary.IsEmpty())
			{
				Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
				[
					SNew(STextBlock).Font(HintFont()).AutoWrapText(true).Text(Summary)
				];
			}

			Box->AddSlot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
			[
				MakeHint(LOCTEXT("DepotsHint", "You can also add depots yourself (Depot ID and platform) in Project Settings > Plugins > Sandwich Steam - Publish."),
					LOCTEXT("OpenSettings", "Open settings"), []() { OpenSettings(GetDefault<USteamPublishSettings>()); })
			];
			return Box;
		}

		TSharedPtr<SVerticalBox> Content;
		FSteamSetupInputs Inputs;
		TArray<FSteamSetupStep> Steps;

		TOptional<int32> PendingAppId;
		TOptional<FString> PendingUsername;

		bool bRefreshRequested = false;
		bool bShowInNav = false;
		bool bAutoLoginTried = false;
		bool bAutoFetchTried = false;

		FDelegateHandle ObjectChangedHandle;
		FDelegateHandle DefinitionChangedHandle;
	};
}

namespace SandwichSteam::Editor
{
	void RegisterSetupDashboardPage()
	{
		const TSharedRef<TWeakPtr<SSteamSetupPage>> PageRef = MakeShared<TWeakPtr<SSteamSetupPage>>();

		FSteamDashboardPage Page;
		Page.Id = TEXT("Setup");
		Page.Label = LOCTEXT("PageLabel", "Setup");
		Page.Icon = FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16");
		Page.Order = 1;
		Page.BuildContent = [PageRef]() -> TSharedRef<SWidget>
		{
			const TSharedRef<SSteamSetupPage> Panel = SNew(SSteamSetupPage);
			*PageRef = Panel;
			return Panel;
		};
		Page.IsVisible = [PageRef]()
		{
			const TSharedPtr<SSteamSetupPage> Panel = PageRef->Pin();
			return Panel.IsValid() && Panel->ShouldShowInNav();
		};
		// The SteamCMD jobs started here belong to the shared service; the SteamCMD page's CanClose already asks about them.
		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
