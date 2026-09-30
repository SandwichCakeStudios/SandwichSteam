// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Assets/AssetDefinition_SteamAppDefinition.h"
#include "Data/SteamAppDefinition.h"

#define LOCTEXT_NAMESPACE "SandwichSteamAppDefinition"

FText UAssetDefinition_SteamAppDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Steam App Definition");
}

FLinearColor UAssetDefinition_SteamAppDefinition::GetAssetColor() const
{
	// Steam blue.
	return FLinearColor(FColor(27, 40, 56));
}

TSoftClassPtr<UObject> UAssetDefinition_SteamAppDefinition::GetAssetClass() const
{
	return USteamAppDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_SteamAppDefinition::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("SteamCategory", "Steam")) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
