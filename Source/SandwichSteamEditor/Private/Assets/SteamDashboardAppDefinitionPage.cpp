// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Assets/SteamDashboardAppDefinitionPage.h"
#include "Core/SteamToolSettings.h"
#include "Dashboard/SteamDashboardRegistry.h"
#include "Data/SteamAppDefinition.h"
#include "Editor.h"
#include "Import/SteamSchemaImporter.h"
#include "Input/SteamInputActionsFile.h"
#include "Presence/SteamPresenceLocalization.h"
#include "Style/SteamToolStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "Validation/SteamProjectValidator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamDashboardAppDefinition"

namespace
{
	TSharedRef<SWidget> MakeActionButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick)
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

	FText MakeRowCountText(const TCHAR* Label, int32 Count)
	{
		return FText::Format(LOCTEXT("RowCount", "{0}: {1}"), FText::FromString(Label), Count);
	}

	/**
	 * Summary + Open asset + the three actions. Rebuilds when the assigned definition changes (reassigned in the
	 * settings, or its rows edited) so the counts stay live while the dashboard tab is open.
	 */
	class SSteamDashboardAppDefinitionPage : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSteamDashboardAppDefinitionPage) {}
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

			ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SSteamDashboardAppDefinitionPage::HandleObjectChanged);
			DefinitionChangedHandle = USteamToolSettings::OnAppDefinitionChanged().AddSP(this, &SSteamDashboardAppDefinitionPage::Refresh);

			Refresh();
		}

		virtual ~SSteamDashboardAppDefinitionPage() override
		{
			FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
			USteamToolSettings::OnAppDefinitionChanged().Remove(DefinitionChangedHandle);
		}

	private:
		void HandleObjectChanged(UObject* Object, FPropertyChangedEvent& /*Event*/)
		{
			const USteamToolSettings* Settings = USteamToolSettings::Get();
			if (Object == GetDefault<USteamToolSettings>() || (Settings && Object == Settings->LoadAppDefinition()))
			{
				Refresh();
			}
		}

		TSharedRef<SWidget> BuildSummary(USteamAppDefinition* Definition)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Definition->GetName())).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 8.f)
				[
					SNew(SWrapBox).UseAllottedSize(true)
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("Stats"), Definition->Stats.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("Achievements"), Definition->Achievements.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("Leaderboards"), Definition->Leaderboards.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("Presence"), Definition->Presence.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("DLC"), Definition->DLC.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("Sessions"), Definition->Sessions.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 16.f, 4.f) [ SNew(STextBlock).Text(MakeRowCountText(TEXT("Input Sets"), Definition->InputSets.Num())).Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f)) ]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeActionButton(LOCTEXT("OpenAsset", "Open asset"), LOCTEXT("OpenAssetTip", "Opens the assigned Steam App Definition in its own editor."),
						[WeakDefinition = TWeakObjectPtr<USteamAppDefinition>(Definition)]()
						{
							USteamAppDefinition* Pinned = WeakDefinition.Get();
							if (Pinned && GEditor)
							{
								GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Pinned);
							}
						})
				];
		}

		TSharedRef<SWidget> BuildNoDefinition()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
					.Text(LOCTEXT("NoDefinition", "No Steam App Definition assigned. Stats, Achievements, Leaderboards, Presence, DLC, Sessions and Steam Input rows all live in one such asset."))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
				[
					MakeActionButton(LOCTEXT("CreateAndAssign", "Create and assign"), LOCTEXT("CreateAndAssignTip", "Creates /Game/Steam/DA_SteamAppDefinition, saves it and assigns it in the Sandwich Steam settings."),
						[]() { FSteamProjectValidator::CreateAndAssignAppDefinition(); })
				];
		}

		void Refresh()
		{
			Content->ClearChildren();

			const USteamToolSettings* Settings = USteamToolSettings::Get();
			USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;

			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
			[
				SNew(STextBlock).Text(LOCTEXT("PageTitle", "App Definition")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("HeadingExtraSmallText"), 1.5f))
			];

			Content->AddSlot().AutoHeight()
			[
				SNew(SBorder).BorderImage(FSteamToolStyle::Get().GetBrush("SandwichSteam.Card")).Padding(10.f)
				[
					Definition ? BuildSummary(Definition) : BuildNoDefinition()
				]
			];

			Content->AddSlot().AutoHeight().Padding(0.f, 12.f, 0.f, 4.f)
			[
				SNew(STextBlock).Text(LOCTEXT("ActionsTitle", "Actions")).Font(FSteamToolStyle::ScaleFont(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"), 1.5f))
			];

			Content->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(LOCTEXT("ActionsHint", "These also exist as buttons inside the asset's own Details panel; use them here to reach them without opening it."))
			];

			Content->AddSlot().AutoHeight()
			[
				SNew(SWrapBox).UseAllottedSize(true)
				+ SWrapBox::Slot().Padding(0.f, 0.f, 6.f, 4.f)
				[
					MakeActionButton(LOCTEXT("ImportButton", "Import from Steam..."),
						LOCTEXT("ImportTip", "Reads Saved/SandwichSteam/Schema_<AppId>.json (Steam.Stats.ExportSchema in Standalone) and merges the achievements into the assigned Steam App Definition."),
						[]() { SandwichSteam::Editor::ImportFromSteam(nullptr); })
				]
				+ SWrapBox::Slot().Padding(0.f, 0.f, 6.f, 4.f)
				[
					MakeActionButton(LOCTEXT("InputButton", "Generate Steam Input Actions File..."),
						LOCTEXT("InputTip", "Writes game_actions_<AppId>.vdf from the Input Sets of the assigned Steam App Definition."),
						[]() { SandwichSteam::Editor::GenerateSteamInputActions(nullptr); })
				]
				+ SWrapBox::Slot().Padding(0.f, 0.f, 6.f, 4.f)
				[
					MakeActionButton(LOCTEXT("PresenceButton", "Generate Rich Presence Localization..."),
						LOCTEXT("PresenceTip", "Writes richpresence_<AppId>.vdf from the Presence rows of the assigned Steam App Definition."),
						[]() { SandwichSteam::Editor::GeneratePresenceLocalization(nullptr); })
				]
			];
		}

		TSharedPtr<SVerticalBox> Content;
		FDelegateHandle ObjectChangedHandle;
		FDelegateHandle DefinitionChangedHandle;
	};
}

namespace SandwichSteam::Editor
{
	void RegisterAppDefinitionDashboardPage()
	{
		FSteamDashboardPage Page;
		Page.Id = TEXT("AppDefinition");
		Page.Label = LOCTEXT("PageLabel", "App Definition");
		Page.Icon = FSlateIcon(FSteamToolStyle::GetStyleSetName(), "SandwichSteam.Icon16");
		Page.Order = 5;
		Page.BuildContent = []() -> TSharedRef<SWidget> { return SNew(SSteamDashboardAppDefinitionPage); };
		RegisterDashboardPage(MoveTemp(Page));
	}
}

#undef LOCTEXT_NAMESPACE
