// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Advanced/SSteamAdvancedPage.h"
#include "Core/SteamToolSettings.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Settings/SteamConfigureAction.h"
#include "Settings/SteamIniWriter.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDashboardAdvanced"

namespace
{
	const FName AdvancedPageId(TEXT("Advanced"));

	/** One heading + card on the Advanced page. Adding a category is one more entry in GetCategories(). */
	struct FSteamAdvancedCategory
	{
		FText Heading;
		TFunction<TSharedRef<SWidget>()> BuildContent;
	};

	FSlateFontInfo BodyFont()
	{
		return FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f);
	}

	FSlateFontInfo HeadingFont()
	{
		return FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f);
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
				SNew(STextBlock).Text(Label).Font(BodyFont())
			];
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

	/** One location: a label, then Create and Open folder. The folder is resolved when clicked. */
	TSharedRef<SWidget> MakeAppIdFileRow(const FText& Label, const FText& CreateToolTip, TFunction<FString()> GetDirectory)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Label).Font(BodyFont()).ToolTipText(CreateToolTip)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
			[
				MakeButton(LOCTEXT("AppIdFileCreate", "Create"), CreateToolTip, [GetDirectory]() { CreateSteamAppIdFile(GetDirectory()); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
			[
				MakeButton(LOCTEXT("AppIdFileOpenFolder", "Open folder"), LOCTEXT("AppIdFileOpenFolderTip", "Opens this folder in the file explorer."),
					[GetDirectory]()
					{
						const FString Directory = GetDirectory();
						IFileManager::Get().MakeDirectory(*Directory, true);
						FPlatformProcess::ExploreFolder(*Directory);
					})
			];
	}

	TSharedRef<SWidget> BuildAppIdFileCategory()
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(BodyFont())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LOCTEXT("AppIdFileIntro", "Normally you do not need this. In Standalone and Development builds the engine writes steam_appid.txt itself and deletes it when the game exits. Use these only when Steam does not recognise your game. Development only: never ship the file (the Publish tool always leaves it out)."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				MakeAppIdFileRow(LOCTEXT("AppIdFileEngine", "Engine Binaries"),
					FText::Format(LOCTEXT("AppIdFileEngineTip", "Writes steam_appid.txt into the folder of the running editor executable ({0}). Affects every project on this engine install, and the engine deletes it when a game started from the editor exits."), FText::FromString(GetEditorBinariesDir())),
					[]() { return GetEditorBinariesDir(); })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeAppIdFileRow(LOCTEXT("AppIdFileProject", "Project Binaries"),
					FText::Format(LOCTEXT("AppIdFileProjectTip", "Writes steam_appid.txt into {0}, next to the game executable of this project (packaged Development builds started outside Steam)."), FText::FromString(GetProjectBinariesDir())),
					[]() { return GetProjectBinariesDir(); })
			];
	}

	/** Writes the settings object to DefaultGame.ini (the settings class is DefaultConfig). */
	void SaveSettings(USteamToolSettings* Settings)
	{
		Settings->TryUpdateDefaultConfigFile();
	}

	/** Label on the left, value editor on the right. */
	TSharedRef<SWidget> MakeTuningRow(const FText& Label, const FText& ToolTip, const TSharedRef<SWidget>& Editor)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Label).Font(BodyFont()).ToolTipText(ToolTip)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SBox).WidthOverride(140.f)
				[
					Editor
				]
			];
	}

	/** An int32 setting edited in place and saved when the value is committed. */
	TSharedRef<SWidget> MakeTuningIntRow(const FText& Label, const FText& ToolTip, int32 MinValue, int32 MaxValue, int32 USteamToolSettings::* Member)
	{
		USteamToolSettings* Settings = GetMutableDefault<USteamToolSettings>();
		return MakeTuningRow(Label, ToolTip,
			SNew(SSpinBox<int32>)
			.MinValue(MinValue)
			.MaxSliderValue(MaxValue)
			.Delta(1000)
			.ToolTipText(ToolTip)
			.IsEnabled_Lambda([Settings]() { return Settings->bWriteNetworkTuning; })
			.Value_Lambda([Settings, Member]() { return Settings->*Member; })
			.OnValueChanged_Lambda([Settings, Member](int32 NewValue) { Settings->*Member = NewValue; })
			.OnValueCommitted_Lambda([Settings, Member, MinValue](int32 NewValue, ETextCommit::Type)
			{
				Settings->*Member = FMath::Max(NewValue, MinValue);
				SaveSettings(Settings);
			}));
	}

	/** The lines Configure Steam will write, one per row, from the current values. */
	FText DescribeTuningLines()
	{
		const USteamToolSettings* Settings = USteamToolSettings::Get();
		if (!Settings || !Settings->bWriteNetworkTuning)
		{
			return LOCTEXT("TuningOff", "Off: nothing is written, the engine defaults or your own values are used.");
		}

		const FSteamNetworkTuning Tuning = SandwichSteam::Editor::MakeNetworkTuning(*Settings);
		FString Text;
		for (const FSteamIniEntry& Entry : FSteamIniWriter::BuildNetworkEngineEntries(Tuning))
		{
			Text += FString::Printf(TEXT("DefaultEngine.ini  [%s] %s=%s\n"), *Entry.Section, *Entry.Key, *Entry.Value);
		}
		for (const FSteamIniEntry& Entry : FSteamIniWriter::BuildNetworkGameEntries(Tuning))
		{
			Text += FString::Printf(TEXT("DefaultGame.ini  [%s] %s=%s\n"), *Entry.Section, *Entry.Key, *Entry.Value);
		}

		FString Problem;
		if (!FSteamIniWriter::IsNetworkTuningConsistent(Tuning, Problem))
		{
			Text += TEXT("\nWarning: ") + Problem;
		}
		return FText::FromString(Text.TrimEnd());
	}

	TSharedRef<SWidget> BuildNetworkTuningCategory()
	{
		USteamToolSettings* Settings = GetMutableDefault<USteamToolSettings>();

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(BodyFont())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LOCTEXT("TuningIntro", "Raises the engine's bandwidth caps a bit and sets the connect timeout, for SteamSockets sessions. The client limit (ConfiguredInternetSpeed) has to match the server limit or the lower one wins, so one value feeds all of them. These are starting points: tune them for your game's replication and player count. Applied by Configure Steam, which shows the changes first."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				MakeTuningRow(LOCTEXT("TuningEnable", "Write network tuning"),
					LOCTEXT("TuningEnableTip", "Off (default): the plugin neither writes nor checks these keys."),
					SNew(SCheckBox)
					.IsChecked_Lambda([Settings]() { return Settings->bWriteNetworkTuning ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([Settings](ECheckBoxState State)
					{
						Settings->bWriteNetworkTuning = State == ECheckBoxState::Checked;
						SaveSettings(Settings);
					}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				MakeTuningIntRow(LOCTEXT("TuningPerClient", "Per-client bandwidth (B/s)"),
					LOCTEXT("TuningPerClientTip", "MaxClientRate, MaxInternetClientRate, ConfiguredInternetSpeed, ConfiguredLanSpeed and MaxDynamicBandwidth."),
					10000, 1000000, &USteamToolSettings::NetBandwidthPerClient)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				MakeTuningIntRow(LOCTEXT("TuningTotal", "Total bandwidth (B/s)"),
					LOCTEXT("TuningTotalTip", "TotalNetBandwidth: shared by all connections. Written as is, set it for the player count of your game."),
					10000, 10000000, &USteamToolSettings::TotalNetBandwidth)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				MakeTuningIntRow(LOCTEXT("TuningMin", "Minimum per connection (B/s)"),
					LOCTEXT("TuningMinTip", "MinDynamicBandwidth: the least a connection gets when the total is shared."),
					1000, 200000, &USteamToolSettings::MinDynamicBandwidth)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				MakeTuningRow(LOCTEXT("TuningTimeout", "Connect timeout (s)"),
					LOCTEXT("TuningTimeoutTip", "InitialConnectTimeout: lower fails faster, higher forgives slow Steam relay connections."),
					SNew(SSpinBox<float>)
					.MinValue(5.f)
					.MaxSliderValue(300.f)
					.Delta(5.f)
					.IsEnabled_Lambda([Settings]() { return Settings->bWriteNetworkTuning; })
					.Value_Lambda([Settings]() { return Settings->InitialConnectTimeout; })
					.OnValueChanged_Lambda([Settings](float NewValue) { Settings->InitialConnectTimeout = NewValue; })
					.OnValueCommitted_Lambda([Settings](float NewValue, ETextCommit::Type)
					{
						Settings->InitialConnectTimeout = FMath::Max(NewValue, 5.f);
						SaveSettings(Settings);
					}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(BodyFont())
				.Text_Lambda([]() { return DescribeTuningLines(); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[
				MakeButton(LOCTEXT("TuningConfigure", "Configure Steam..."), LOCTEXT("TuningConfigureTip", "Shows the pending ini changes and writes them."),
					[]() { SandwichSteam::Editor::ConfigureSteam(); })
			];
	}

	TArray<FSteamAdvancedCategory> GetCategories()
	{
		return {
			{ LOCTEXT("TuningHeading", "Networking (tuning)"), &BuildNetworkTuningCategory },
			{ LOCTEXT("AppIdFileHeading", "steam_appid.txt (troubleshooting)"), &BuildAppIdFileCategory }
		};
	}

	/** Page title, then one heading + card per category. Built once; nothing on it polls. */
	class SSteamAdvancedPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSteamAdvancedPage) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& /*InArgs*/)
		{
			const TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(LOCTEXT("PageTitle", "Advanced")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
			];

			for (const FSteamAdvancedCategory& Category : GetCategories())
			{
				Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(STextBlock).Text(Category.Heading).Font(HeadingFont())
				];

				Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
				[
					SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
					[
						Category.BuildContent()
					]
				];
			}

			ChildSlot
			[
				SNew(SBox).Padding(4.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						Content
					]
				]
			];
		}
	};
}

namespace SandwichSteam::Editor
{
	void RegisterAdvancedDashboardPage()
	{
		FSteamDashboardPage Page;
		Page.Id = AdvancedPageId;
		Page.Label = LOCTEXT("PageLabel", "Advanced");
		Page.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings");
		Page.Order = 100;
		Page.bPinToBottom = true;
		Page.BuildContent = []() -> TSharedRef<SWidget> { return SNew(SSteamAdvancedPage); };
		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
