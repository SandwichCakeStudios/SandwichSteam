// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Assets/SteamAppDefinitionFactory.h"
#include "Data/SteamAppDefinition.h"

#define LOCTEXT_NAMESPACE "SandwichSteamAppDefinitionFactory"

USteamAppDefinitionFactory::USteamAppDefinitionFactory()
{
	SupportedClass = USteamAppDefinition::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* USteamAppDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USteamAppDefinition>(InParent, InClass, InName, Flags | RF_Transactional);
}

FText USteamAppDefinitionFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Steam App Definition");
}

#undef LOCTEXT_NAMESPACE
