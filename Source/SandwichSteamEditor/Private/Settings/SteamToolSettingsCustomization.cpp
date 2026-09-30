// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SteamToolSettingsCustomization.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamToolSettings.h"
#include "Dashboard/SSteamDashboardPanel.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamSettingsCustomization"

namespace
{
	struct FFeatureEntry
	{
		FGameplayTag Tag;
		FString Name;
		FText Scope;
		FText ScopeTip;
	};

	/** Every installed feature subsystem class, read from its default object (tag and scope). */
	TArray<FFeatureEntry> CollectFeatures()
	{
		TArray<FFeatureEntry> Entries;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			const UClass* Class = *It;
			if (!Class->IsChildOf(USteamFeatureSubsystem::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

			const USteamFeatureSubsystem* Default = Class->GetDefaultObject<USteamFeatureSubsystem>();
			const FGameplayTag Tag = Default ? Default->GetFeatureTag() : FGameplayTag();
			if (!Tag.IsValid())
			{
				continue;
			}

			FFeatureEntry Entry;
			Entry.Tag = Tag;
			Entry.Name = Tag.ToString();
			Entry.Name.RemoveFromStart(TEXT("Steam.Feature."));

			switch (Default->GetFeatureScope())
			{
			case ESteamFeatureScope::ClientOnly:
				Entry.Scope = LOCTEXT("ScopeClient", "Client");
				Entry.ScopeTip = LOCTEXT("ScopeClientTip", "Created on clients and standalone games, never on a dedicated server.");
				break;
			case ESteamFeatureScope::ServerOnly:
				Entry.Scope = LOCTEXT("ScopeServer", "Server");
				Entry.ScopeTip = LOCTEXT("ScopeServerTip", "Created on dedicated servers only.");
				break;
			default:
				Entry.Scope = LOCTEXT("ScopeBoth", "Client + Server");
				Entry.ScopeTip = LOCTEXT("ScopeBothTip", "Created on clients and on dedicated servers.");
				break;
			}

			Entries.Add(MoveTemp(Entry));
		}

		Entries.Sort([](const FFeatureEntry& A, const FFeatureEntry& B) { return A.Name < B.Name; });
		return Entries;
	}
}

TSharedRef<IDetailCustomization> FSteamToolSettingsCustomization::MakeInstance()
{
	return MakeShared<FSteamToolSettingsCustomization>();
}

void FSteamToolSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& StatusCategory = DetailBuilder.EditCategory("Status", LOCTEXT("StatusCategory", "Status"), ECategoryPriority::Important);
	StatusCategory.AddCustomRow(LOCTEXT("StatusFilter", "status setup validation configure steam dashboard"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("StatusMoved", "Setup status, validation and quick fixes now live in the Steam Dashboard."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("OpenDashboardTip", "Opens the Steam Dashboard: setup status, the App Definition and Publish, in one tab."))
			.OnClicked_Lambda([]()
			{
				SandwichSteam::Editor::SandwichSteamDashboard();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("OpenDashboard", "Open Steam Dashboard"))
			]
		]
	];

	// One checkbox per installed feature instead of a raw tag container.
	const TSharedRef<IPropertyHandle> DisabledHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USteamToolSettings, DisabledFeatures));
	DetailBuilder.HideProperty(DisabledHandle);

	IDetailCategoryBuilder& FeatureCategory = DetailBuilder.EditCategory("Features", LOCTEXT("FeaturesCategory", "Features"));
	FeatureCategory.AddCustomRow(LOCTEXT("FeaturesHintFilter", "features enable disable"))
	.WholeRowContent()
	[
		SNew(STextBlock)
		.AutoWrapText(true)
		.Text(LOCTEXT("FeaturesHint", "Unchecked features are not created at runtime and cost nothing. Restart the game or editor session after changing them. To remove a feature from the package completely, delete its module."))
	];

	for (const FFeatureEntry& Feature : CollectFeatures())
	{
		const FGameplayTag Tag = Feature.Tag;

		FeatureCategory.AddCustomRow(FText::FromString(Feature.Name))
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Feature.Name))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Feature.Scope)
				.ToolTipText(Feature.ScopeTip)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.ToolTipText(LOCTEXT("FeatureToggleTip", "Checked: the feature is created at runtime."))
			.IsChecked_Lambda([Tag]()
			{
				const USteamToolSettings* Settings = USteamToolSettings::Get();
				return (!Settings || Settings->IsFeatureEnabled(Tag)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Tag, DisabledHandle](ECheckBoxState NewState)
			{
				// Going through the property handle records the change for undo and saves the settings like any edit.
				DisabledHandle->NotifyPreChange();
				USteamToolSettings* Settings = GetMutableDefault<USteamToolSettings>();
				if (NewState == ECheckBoxState::Checked)
				{
					Settings->DisabledFeatures.RemoveTag(Tag);
				}
				else
				{
					Settings->DisabledFeatures.AddTag(Tag);
				}
				DisabledHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			})
		];
	}
}

#undef LOCTEXT_NAMESPACE
