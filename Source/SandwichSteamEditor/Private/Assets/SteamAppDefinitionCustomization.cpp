// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Assets/SteamAppDefinitionCustomization.h"
#include "Data/SteamAppDefinition.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Import/SteamSchemaImporter.h"
#include "Input/SteamInputActionsFile.h"
#include "Presence/SteamPresenceLocalization.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamAppDefinitionDetails"

TSharedRef<IDetailCustomization> FSteamAppDefinitionCustomization::MakeInstance()
{
	return MakeShared<FSteamAppDefinitionCustomization>();
}

void FSteamAppDefinitionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	const TWeakObjectPtr<USteamAppDefinition> Definition = Objects.Num() == 1 ? Cast<USteamAppDefinition>(Objects[0].Get()) : nullptr;

	IDetailCategoryBuilder& ImportCategory = DetailBuilder.EditCategory("Import", LOCTEXT("ImportCategory", "Import from Steam"), ECategoryPriority::Important);

	ImportCategory.AddCustomRow(LOCTEXT("ImportFilter", "import steam achievements schema"))
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("ImportHint", "Run the game in Standalone, type Steam.Debug.Show and click \"Export schema\". Then import: achievements are merged by API name, existing tags and progress settings are kept, nothing is deleted. Stats cannot be listed by Steam and stay manual."))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SButton)
			.IsEnabled(Definition.IsValid())
			.ToolTipText(LOCTEXT("ImportTip", "Reads Saved/SandwichSteam/Schema_<AppId>.json and merges the achievements into this asset."))
			.OnClicked_Lambda([Definition]()
			{
				SandwichSteam::Editor::ImportFromSteam(Definition.Get());
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("ImportButton", "Import from Steam..."))
			]
		]
	];

	IDetailCategoryBuilder& PresenceCategory = DetailBuilder.EditCategory("RichPresence", LOCTEXT("PresenceCategory", "Rich Presence Localization"), ECategoryPriority::Important);

	PresenceCategory.AddCustomRow(LOCTEXT("PresenceFilter", "rich presence localization vdf generate"))
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("PresenceHint", "Writes the localization file for the Presence rows (token and text, one block per Steam language) that you upload on the Steamworks partner site. Localize the Localized Text of each row like any text; write the keys in braces, for example \"Playing on {map}\"."))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SButton)
			.IsEnabled(Definition.IsValid())
			.ToolTipText(LOCTEXT("PresenceTip", "Writes richpresence_<AppId>.vdf into the Data Directory of the Sandwich Steam settings."))
			.OnClicked_Lambda([Definition]()
			{
				SandwichSteam::Editor::GeneratePresenceLocalization(Definition.Get());
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("PresenceButton", "Generate Rich Presence Localization..."))
			]
		]
	];

	IDetailCategoryBuilder& InputCategory = DetailBuilder.EditCategory("SteamInputActions", LOCTEXT("InputCategory", "Steam Input Actions File"), ECategoryPriority::Important);

	InputCategory.AddCustomRow(LOCTEXT("InputFilter", "steam input actions vdf game_actions generate"))
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("InputHint", "Writes the Steam Input action file for the Input Sets (sets and layers with their actions) that you upload on the Steamworks partner site. To test with an App ID you do not own, set it as the Action Manifest in the Sandwich Steam - Input settings."))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SButton)
			.IsEnabled(Definition.IsValid())
			.ToolTipText(LOCTEXT("InputTip", "Writes game_actions_<AppId>.vdf into the Data Directory of the Sandwich Steam settings."))
			.OnClicked_Lambda([Definition]()
			{
				SandwichSteam::Editor::GenerateSteamInputActions(Definition.Get());
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("InputButton", "Generate Steam Input Actions File..."))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
