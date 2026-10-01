// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Dashboard/SSteamDashboardPanel.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Settings/SSteamStatusPanel.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDashboardPanel"

namespace
{
	const FName DashboardTabName(TEXT("SandwichSteamDashboardTab"));
	constexpr float NavWidth = 240.f;
	constexpr float StatusWidth = 340.f;

	/** The open dashboard (it is a nomad tab, so there is at most one), for OpenDashboardPage. */
	TWeakPtr<SSteamDashboardPanel> OpenPanel;
}

void SSteamDashboardPanel::Construct(const FArguments& /*InArgs*/)
{
	const TArray<FSteamDashboardPage> DashboardPages = SandwichSteam::Editor::GetDashboardPages();

	const TSharedRef<SVerticalBox> Nav = SNew(SVerticalBox);
	SAssignNew(Pages, SWidgetSwitcher).WidgetIndex_Lambda([this]() { return GetDisplayedPage(); });

	for (int32 Index = 0; Index < DashboardPages.Num(); ++Index)
	{
		const FSteamDashboardPage& Page = DashboardPages[Index];
		PageIds.Add(Page.Id);
		PageVisibility.Add(Page.IsVisible);

		Nav->AddSlot().AutoHeight()
		[
			SNew(SBox)
			.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			.Visibility_Lambda([this, Index]() { return IsPageVisible(Index) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				MakeNavButton(Index, Page)
			]
		];

		Pages->AddSlot()
		[
			Page.BuildContent ? Page.BuildContent() : SNullWidget::NullWidget
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		// Header
		+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 10.f, 12.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(SImage).Image(FSteamToolStyle::Get().GetBrush("SandwichSteam.Icon40"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Title", "Steam Dashboard")).Font(FAppStyle::GetFontStyle("HeadingExtraSmallText"))
			]
		]
		// Body: nav | page | status
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(12.f, 0.f, 12.f, 12.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBox).WidthOverride(NavWidth)
				[
					SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(6.f)
					[
						Nav
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 12.f, 0.f)
			[
				Pages.ToSharedRef()
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(StatusWidth)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SSteamStatusPanel)
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SSteamDashboardPanel::MakeNavButton(int32 PageIndex, const FSteamDashboardPage& Page)
{
	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
		.IsChecked_Lambda([this, PageIndex]() { return GetDisplayedPage() == PageIndex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this, PageIndex](ECheckBoxState) { ActivePage = PageIndex; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 6.f, 10.f, 6.f)
			[
				SNew(SImage).Image(Page.Icon.GetIcon())
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Page.Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
				.Margin(FMargin(0.f, 6.f, 10.f, 6.f))
			]
		];
}

bool SSteamDashboardPanel::IsPageVisible(int32 PageIndex) const
{
	return PageVisibility.IsValidIndex(PageIndex) && (!PageVisibility[PageIndex] || PageVisibility[PageIndex]());
}

int32 SSteamDashboardPanel::GetDisplayedPage() const
{
	if (IsPageVisible(ActivePage))
	{
		return ActivePage;
	}
	for (int32 Index = 0; Index < PageVisibility.Num(); ++Index)
	{
		if (IsPageVisible(Index))
		{
			return Index;
		}
	}
	return ActivePage;
}

bool SSteamDashboardPanel::SelectPage(FName PageId)
{
	const int32 Index = PageIds.IndexOfByKey(PageId);
	if (!IsPageVisible(Index))
	{
		return false;
	}
	ActivePage = Index;
	return true;
}

namespace SandwichSteam::Editor
{
	void RegisterDashboardTab()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DashboardTabName, FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			const TSharedRef<SSteamDashboardPanel> Panel = SNew(SSteamDashboardPanel);
			OpenPanel = Panel;

			const TSharedRef<SDockTab> Tab = SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("DashboardTabLabel", "Steam Dashboard"))
				.OnCanCloseTab_Lambda([]()
				{
					for (const FSteamDashboardPage& Page : GetDashboardPages())
					{
						if (Page.CanClose && !Page.CanClose())
						{
							return false;
						}
					}
					return true;
				})
				[
					Panel
				];
			return Tab;
		}))
		.SetDisplayName(LOCTEXT("DashboardTabDisplayName", "Steam Dashboard"))
		.SetTooltipText(LOCTEXT("DashboardTabTooltip", "One place for Steam setup, the App Definition and publishing."))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16"));
	}

	void UnregisterDashboardTab()
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DashboardTabName);
		}
	}

	void SandwichSteamDashboard()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(DashboardTabName);
	}

	void OpenDashboardPage(FName PageId)
	{
		SandwichSteamDashboard();
		if (const TSharedPtr<SSteamDashboardPanel> Panel = OpenPanel.Pin())
		{
			Panel->SelectPage(PageId);
		}
	}
}

#undef LOCTEXT_NAMESPACE
