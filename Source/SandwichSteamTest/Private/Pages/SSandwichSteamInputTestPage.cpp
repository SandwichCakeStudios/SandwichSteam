// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamInputTestPage.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Engine/Texture2D.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamInputLibrary.h"
#include "SteamInputSubsystem.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestInput"

void SSandwichSteamInputTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Needs a controller and a Steam Input action file: generate it from the App Definition (Generate Steam Input Actions File) and set it as the Action Manifest in the Input settings when testing with App ID 480. Run in Standalone.");

	GlyphBrush.DrawAs = ESlateBrushDrawType::Image;
	GlyphBrush.ImageSize = FVector2D(48.0, 48.0);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Input", "Input")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamInputSubsystem* Input = GetInput();
			return Input ? SandwichSteamTest::BoolText(Input->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Controllers", "Controllers"), TAttribute<FText>::CreateLambda([this] { return BuildControllersText(); })) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Set", "Action set"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamInputSubsystem* Input = GetInput();
			return Input && Input->GetActionSet().IsValid() ? FText::FromName(Input->GetActionSet().GetTagName()) : LOCTEXT("NoSet", "none");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Layers", "Layers on"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamInputSubsystem* Input = GetInput();
			return Input ? FText::AsNumber(Input->GetActiveLayers().Num()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildStateText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("NextSet", "Next action set"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnNextSet)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("NextSetByName", "Next set by name"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnNextSetByName)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ToggleLayer", "Toggle first layer"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnToggleLayer)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Rumble", "Rumble 0.5 s"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnRumble)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("StopRumble", "Stop rumble"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnStopRumble)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LedRed", "Light bar red"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnLedRed)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LedReset", "Light bar reset"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnLedReset)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Panel", "Binding panel"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnBindingPanel)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Glyph", "Glyph of first action"), FOnClicked::CreateSP(this, &SSandwichSteamInputTestPage::OnShowGlyph)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBox).WidthOverride(48.f).HeightOverride(48.f)
				[
					SNew(SImage).Image(&GlyphBrush)
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text_Lambda([this] { return GlyphLabel; })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)))
			.Text_Lambda([this] { return Status; })
		]
	];
}

USteamInputSubsystem* SSandwichSteamInputTestPage::GetInput() const
{
	return USteamInputSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamInputTestPage::BuildStateText() const
{
#if SANDWICHSTEAM_WITH_DEBUG
	const USteamInputSubsystem* Input = GetInput();
	return Input ? FText::FromString(Input->BuildDebugString()) : FText::GetEmpty();
#else
	return FText::GetEmpty();
#endif
}

FText SSandwichSteamInputTestPage::BuildControllersText() const
{
	const USteamInputSubsystem* Input = GetInput();
	if (!Input || !Input->IsFeatureActive())
	{
		return FText::GetEmpty();
	}

	const TArray<FSteamInputController> Controllers = Input->GetControllers();
	if (Controllers.IsEmpty())
	{
		return LOCTEXT("None", "none connected");
	}

	FString Text;
	for (const FSteamInputController& Controller : Controllers)
	{
		Text += FString::Printf(TEXT("[%d] %s   "), Controller.Slot, *StaticEnum<ESteamInputControllerType>()->GetNameStringByValue(static_cast<int64>(Controller.Type)));
	}
	return FText::FromString(Text.TrimEnd());
}

void SSandwichSteamInputTestPage::ReportResult(const FSteamResult& Result, const FText& Done)
{
	Status = Result.IsSuccess() ? Done : FText::Format(LOCTEXT("Failed", "Failed ({0}): {1}"), FText::FromName(Result.ErrorTag.GetTagName()), Result.Message);
}

FReply SSandwichSteamInputTestPage::OnNextSet()
{
	USteamInputSubsystem* Input = GetInput();
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	const USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;
	if (!Input || !Definition)
	{
		Status = LOCTEXT("NoDefinition", "The Input feature or the Steam App Definition is not available.");
		return FReply::Handled();
	}

	// The next set after the active one, wrapping around (layers are skipped).
	TArray<FGameplayTag> Sets;
	for (const FSteamInputActionSetDef& Set : Definition->InputSets)
	{
		if (!Set.bLayer && Set.Tag.IsValid())
		{
			Sets.Add(Set.Tag);
		}
	}
	if (Sets.IsEmpty())
	{
		Status = LOCTEXT("NoSets", "The App Definition has no action sets (not layers) with a tag.");
		return FReply::Handled();
	}

	const int32 Current = Sets.IndexOfByKey(Input->GetActionSet());
	const FGameplayTag Next = Sets[(Current + 1) % Sets.Num()];
	ReportResult(Input->SetActionSet(Next), FText::Format(LOCTEXT("SetChanged", "Action set {0}."), FText::FromName(Next.GetTagName())));
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnNextSetByName()
{
	// Same as Next action set, through the Blueprint library by the Steam set name.
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	const USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;
	if (!GetInput() || !Definition)
	{
		Status = LOCTEXT("NoDefinitionByName", "The Input feature or the Steam App Definition is not available.");
		return FReply::Handled();
	}

	TArray<FName> Names;
	for (const FSteamInputActionSetDef& Set : Definition->InputSets)
	{
		if (!Set.bLayer && !Set.SteamSetName.IsNone())
		{
			Names.Add(Set.SteamSetName);
		}
	}
	if (Names.IsEmpty())
	{
		Status = LOCTEXT("NoSetsByName", "The App Definition has no action sets (not layers) with a Steam set name.");
		return FReply::Handled();
	}

	const int32 Current = Names.IndexOfByKey(USteamInputLibrary::GetSteamInputActionSetName(WorldContext.Get()));
	const FName Next = Names[(Current + 1) % Names.Num()];
	ReportResult(USteamInputLibrary::SetSteamInputActionSetByName(WorldContext.Get(), Next), FText::Format(LOCTEXT("SetChangedByName", "Action set {0} (by name)."), FText::FromName(Next)));
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnToggleLayer()
{
	USteamInputSubsystem* Input = GetInput();
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	const USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;
	if (!Input || !Definition)
	{
		Status = LOCTEXT("NoDefinitionLayer", "The Input feature or the Steam App Definition is not available.");
		return FReply::Handled();
	}

	for (const FSteamInputActionSetDef& Set : Definition->InputSets)
	{
		if (Set.bLayer && Set.Tag.IsValid())
		{
			const bool bOn = !Input->GetActiveLayers().Contains(Set.Tag);
			ReportResult(bOn ? Input->ActivateLayer(Set.Tag) : Input->DeactivateLayer(Set.Tag),
				FText::Format(bOn ? LOCTEXT("LayerOn", "Layer {0} on.") : LOCTEXT("LayerOff", "Layer {0} off."), FText::FromName(Set.Tag.GetTagName())));
			return FReply::Handled();
		}
	}

	Status = LOCTEXT("NoLayers", "The App Definition has no layer with a tag.");
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnRumble()
{
	if (USteamInputSubsystem* Input = GetInput())
	{
		ReportResult(Input->TriggerVibration(INDEX_NONE, 0.7f, 0.7f, 0.0f, 0.0f, 0.5f), LOCTEXT("Rumbling", "Rumble started on every controller."));
	}
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnStopRumble()
{
	if (USteamInputSubsystem* Input = GetInput())
	{
		ReportResult(Input->StopVibration(), LOCTEXT("Stopped", "Rumble stopped."));
	}
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnLedRed()
{
	if (USteamInputSubsystem* Input = GetInput())
	{
		ReportResult(Input->SetLedColor(INDEX_NONE, FColor::Red), LOCTEXT("LedIsRed", "Light bar set to red (controllers with a light bar only)."));
	}
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnLedReset()
{
	if (USteamInputSubsystem* Input = GetInput())
	{
		ReportResult(Input->ResetLedColor(), LOCTEXT("LedIsReset", "Light bar back to the player's setting."));
	}
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnBindingPanel()
{
	if (USteamInputSubsystem* Input = GetInput())
	{
		ReportResult(Input->ShowBindingPanel(), LOCTEXT("PanelRequested", "Steam's binding panel was requested (look for the Steam overlay)."));
	}
	return FReply::Handled();
}

FReply SSandwichSteamInputTestPage::OnShowGlyph()
{
	USteamInputSubsystem* Input = GetInput();
	const USteamToolSettings* Settings = USteamToolSettings::Get();
	const USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;
	const FSteamInputActionSetDef* Set = (Input && Definition) ? Definition->FindInputSet(Input->GetActionSet()) : nullptr;
	if (!Set || Set->Actions.IsEmpty())
	{
		Status = LOCTEXT("NoAction", "There is no active action set with an action to show.");
		return FReply::Handled();
	}

	FSteamInputGlyph Glyph;
	const FName Action = Set->Actions[0].SteamName;
	const FSteamResult Result = Input->GetGlyphForAction(Action, Glyph);
	ReportResult(Result, FText::Format(LOCTEXT("GlyphShown", "Glyph of {0}."), FText::FromName(Action)));

	GlyphTexture.Reset(Glyph.Texture);
	GlyphBrush.SetResourceObject(Glyph.Texture);
	GlyphLabel = Result.IsSuccess() ? FText::Format(LOCTEXT("GlyphLabel", "{0}: {1}"), FText::FromName(Action), Glyph.Label) : FText::GetEmpty();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
