// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Sdk/SteamDashboardSdkPage.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDashboardSdk"

namespace
{
	struct FSteamworksSdkEntry
	{
		FString FolderName;
		FString FullPath;
	};

	FString GetSteamworksRootDir()
	{
		// Explore folder needs an absolute path: FPaths::EngineDir() is relative to the working directory (for
		// example "../../../Engine/"), which the shell cannot resolve on its own.
		return FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Source/ThirdParty/Steamworks"));
	}

	/** Trailing digits of a "Steamv164"-style folder name, for a latest-first sort. 0 when there are none. */
	int32 ExtractVersionNumber(const FString& FolderName)
	{
		FString Digits;
		for (int32 Index = FolderName.Len() - 1; Index >= 0 && FChar::IsDigit(FolderName[Index]); --Index)
		{
			Digits.InsertAt(0, FolderName[Index]);
		}
		return Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits);
	}

	TArray<FSteamworksSdkEntry> ScanSteamworksSdks()
	{
		TArray<FSteamworksSdkEntry> Entries;

		const FString RootDir = GetSteamworksRootDir();
		TArray<FString> SubDirNames;
		IFileManager::Get().FindFiles(SubDirNames, *(RootDir / TEXT("*")), /*Files*/ false, /*Directories*/ true);

		for (const FString& Name : SubDirNames)
		{
			if (Name.StartsWith(TEXT("Steamv"), ESearchCase::IgnoreCase))
			{
				Entries.Add({Name, RootDir / Name});
			}
		}

		Entries.Sort([](const FSteamworksSdkEntry& A, const FSteamworksSdkEntry& B)
		{
			return ExtractVersionNumber(A.FolderName) > ExtractVersionNumber(B.FolderName);
		});

		return Entries;
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

	/**
	 * Lists the Steamworks SDK versions found under Engine/Source/ThirdParty/Steamworks, each with an Open folder
	 * button, and a short guide for adding a new one. Rescans on construct and on Refresh; nothing watches the
	 * directory, since SDK versions only change when the developer adds one by hand.
	 */
	class SSteamDashboardSdkPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSteamDashboardSdkPage) {}
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

			Refresh();
		}

	private:
		TSharedRef<SWidget> BuildInstalledCard()
		{
			const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
			const TArray<FSteamworksSdkEntry> Entries = ScanSteamworksSdks();

			if (Entries.IsEmpty())
			{
				List->AddSlot().AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					.Text(LOCTEXT("NoneFound", "No Steamworks SDK folders found. See the guide below to add one."))
				];
			}

			for (const FSteamworksSdkEntry& Entry : Entries)
			{
				List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(Entry.FolderName)).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						MakeButton(LOCTEXT("OpenFolder", "Open folder"), LOCTEXT("OpenFolderTip", "Opens this SDK version's folder in the file explorer."),
							[Path = Entry.FullPath]() { FPlatformProcess::ExploreFolder(*Path); })
					]
				];
			}

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
						[
							SNew(STextBlock)
							.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Text(FText::FromString(GetSteamworksRootDir()))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							List
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeButton(LOCTEXT("Refresh", "Refresh"), LOCTEXT("RefreshTip", "Rescans Engine/Source/ThirdParty/Steamworks for SDK versions."),
						[this]() { Refresh(); })
				];
		}

		TSharedRef<SWidget> BuildGuideCard()
		{
			return SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(STextBlock).Text(LOCTEXT("GuideHeading", "Downloading Steamworks")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					.Text(LOCTEXT("GuideBody1", "If your application meets Valve's requirements, go ahead and download the latest version of the Steamworks SDK. Once you have downloaded your desired SDK version, unzip and copy the SDK to ../Engine/Source/ThirdParty/Steamworks/Steam<VERSION>/sdk where <VERSION> is the SDK version number. For example, if you are using Steamworks Version 1.53, the <VERSION> is v153."))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					.Text(LOCTEXT("GuideBody2", "If you're updating your project's Steamworks SDK, make sure to update the SteamVersionNumber in your Steamworks.build.cs file, which is located in the project directory. Updating this value also updates the path for your Steamworks SDK so Unreal Engine uses the correct SDK version."))
				]
			];
		}

		void Refresh()
		{
			Content->ClearChildren();

			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(LOCTEXT("PageTitle", "Steamworks SDK")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
			];

			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
			[
				BuildInstalledCard()
			];

			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
			[
				SNew(STextBlock).Text(LOCTEXT("GuideSection", "Adding a new SDK version")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
			];

			Content->AddSlot().AutoHeight()
			[
				BuildGuideCard()
			];
		}

		TSharedPtr<SVerticalBox> Content;
	};
}

namespace SandwichSteam::Editor
{
	void RegisterSteamworksSdkDashboardPage()
	{
		FSteamDashboardPage Page;
		Page.Id = TEXT("SteamworksSdk");
		Page.Label = LOCTEXT("PageLabel", "Steamworks SDK");
		Page.Icon = FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16");
		Page.Order = 8;
		Page.BuildContent = []() -> TSharedRef<SWidget> { return SNew(SSteamDashboardSdkPage); };
		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
