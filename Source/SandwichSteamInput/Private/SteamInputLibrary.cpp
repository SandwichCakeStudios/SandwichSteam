// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamInputLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamNameWarnings.h"
#include "SteamInputSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "InputLibNotAvailable", "The Input feature is not available in this game instance."));
	}
}

FSteamResult USteamInputLibrary::SetSteamInputActionSet(const UObject* WorldContextObject, FGameplayTag ActionSet)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->SetActionSet(ActionSet) : NotAvailable();
}

FGameplayTag USteamInputLibrary::GetSteamInputActionSet(const UObject* WorldContextObject)
{
	const USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->GetActionSet() : FGameplayTag();
}

FSteamResult USteamInputLibrary::ActivateSteamInputLayer(const UObject* WorldContextObject, FGameplayTag Layer)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->ActivateLayer(Layer) : NotAvailable();
}

FSteamResult USteamInputLibrary::DeactivateSteamInputLayer(const UObject* WorldContextObject, FGameplayTag Layer)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->DeactivateLayer(Layer) : NotAvailable();
}

FSteamResult USteamInputLibrary::SetSteamInputActionSetByName(const UObject* WorldContextObject, FName ActionSetName)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? SandwichSteam::WarnOnceIfInvalid(TEXT("input set"), ActionSetName.ToString(), Input->SetActionSetByName(ActionSetName)) : NotAvailable();
}

FName USteamInputLibrary::GetSteamInputActionSetName(const UObject* WorldContextObject)
{
	const USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->GetActionSetName() : NAME_None;
}

FSteamResult USteamInputLibrary::ActivateSteamInputLayerByName(const UObject* WorldContextObject, FName LayerName)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? SandwichSteam::WarnOnceIfInvalid(TEXT("input layer"), LayerName.ToString(), Input->ActivateLayerByName(LayerName)) : NotAvailable();
}

FSteamResult USteamInputLibrary::DeactivateSteamInputLayerByName(const UObject* WorldContextObject, FName LayerName)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? SandwichSteam::WarnOnceIfInvalid(TEXT("input layer"), LayerName.ToString(), Input->DeactivateLayerByName(LayerName)) : NotAvailable();
}

TArray<FGameplayTag> USteamInputLibrary::GetActiveSteamInputLayers(const UObject* WorldContextObject)
{
	const USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->GetActiveLayers() : TArray<FGameplayTag>();
}

TArray<FSteamInputController> USteamInputLibrary::GetSteamInputControllers(const UObject* WorldContextObject)
{
	const USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->GetControllers() : TArray<FSteamInputController>();
}

bool USteamInputLibrary::HasSteamInputController(const UObject* WorldContextObject)
{
	const USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input && Input->HasController();
}

FSteamResult USteamInputLibrary::GetSteamInputGlyphForAction(const UObject* WorldContextObject, FName Action, FSteamInputGlyph& OutGlyph, int32 Slot)
{
	OutGlyph = FSteamInputGlyph();
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->GetGlyphForAction(Action, OutGlyph, Slot < 0 ? INDEX_NONE : Slot) : NotAvailable();
}

FSteamResult USteamInputLibrary::GetSteamInputGlyphForKey(const UObject* WorldContextObject, FKey Key, FSteamInputGlyph& OutGlyph, int32 Slot)
{
	OutGlyph = FSteamInputGlyph();
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->GetGlyphForKey(Key, OutGlyph, Slot < 0 ? INDEX_NONE : Slot) : NotAvailable();
}

FSteamResult USteamInputLibrary::TriggerSteamInputVibration(const UObject* WorldContextObject, int32 Slot, float Left, float Right, float LeftTrigger, float RightTrigger, float DurationSeconds)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->TriggerVibration(Slot < 0 ? INDEX_NONE : Slot, Left, Right, LeftTrigger, RightTrigger, DurationSeconds) : NotAvailable();
}

FSteamResult USteamInputLibrary::StopSteamInputVibration(const UObject* WorldContextObject, int32 Slot)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->StopVibration(Slot < 0 ? INDEX_NONE : Slot) : NotAvailable();
}

FSteamResult USteamInputLibrary::SetSteamInputLedColor(const UObject* WorldContextObject, int32 Slot, FColor Color)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->SetLedColor(Slot < 0 ? INDEX_NONE : Slot, Color) : NotAvailable();
}

FSteamResult USteamInputLibrary::ResetSteamInputLedColor(const UObject* WorldContextObject, int32 Slot)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->ResetLedColor(Slot < 0 ? INDEX_NONE : Slot) : NotAvailable();
}

FSteamResult USteamInputLibrary::ShowSteamInputBindingPanel(const UObject* WorldContextObject, int32 Slot)
{
	USteamInputSubsystem* Input = USteamInputSubsystem::Get(WorldContextObject);
	return Input ? Input->ShowBindingPanel(Slot < 0 ? INDEX_NONE : Slot) : NotAvailable();
}
