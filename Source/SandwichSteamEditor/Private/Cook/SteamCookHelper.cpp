// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Cook/SteamCookHelper.h"
#include "Core/SteamEngineCompat.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Engine/AssetManagerSettings.h"
#include "SandwichSteamEditor.h"

namespace
{
	/** True when one of the entry's specific assets or directories finds the package. */
	bool IsCoveredByEntry(const FPrimaryAssetTypeInfo& Info, const FSoftObjectPath& AssetPath, const FString& Package)
	{
		bool bCovered = Info.GetSpecificAssets().Contains(AssetPath);
		for (const FDirectoryPath& Directory : Info.GetDirectories())
		{
			bCovered |= !Directory.Path.IsEmpty() && Package.StartsWith(Directory.Path);
		}
		return bCovered;
	}
}

void FSteamCookHelper::Register()
{
	PostEngineInitHandle = SandwichSteam::Compat::OnPostEngineInit().AddLambda([]()
	{
		EnsureAppDefinitionCooked();
	});

	SettingsChangedHandle = USteamToolSettings::OnAppDefinitionChanged().AddRaw(this, &FSteamCookHelper::HandleAppDefinitionChanged);
}

void FSteamCookHelper::Unregister()
{
	if (PostEngineInitHandle.IsValid())
	{
		SandwichSteam::Compat::OnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	if (SettingsChangedHandle.IsValid())
	{
		USteamToolSettings::OnAppDefinitionChanged().Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}
}

void FSteamCookHelper::HandleAppDefinitionChanged()
{
	EnsureAppDefinitionCooked();
}

bool FSteamCookHelper::EnsureAppDefinitionCooked()
{
	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	if (!ToolSettings || ToolSettings->AppDefinition.IsNull())
	{
		return false;
	}

	const FSoftObjectPath DefinitionPath = ToolSettings->AppDefinition.ToSoftObjectPath();
	const FString TypeName = USteamAppDefinition::GetAssetType().ToString();
	const FString DefinitionPackage = DefinitionPath.GetLongPackageName();

	UAssetManagerSettings* AssetManagerSettings = GetMutableDefault<UAssetManagerSettings>();
	if (!AssetManagerSettings)
	{
		return false;
	}

	FPrimaryAssetTypeInfo* Existing = AssetManagerSettings->PrimaryAssetTypesToScan.FindByPredicate([&TypeName](const FPrimaryAssetTypeInfo& Info)
	{
		return Info.PrimaryAssetType.ToString() == TypeName;
	});

	bool bChanged = false;
	if (!Existing)
	{
		FPrimaryAssetTypeInfo Info;
		Info.PrimaryAssetType = FName(*TypeName);
		Info.SetAssetBaseClass(USteamAppDefinition::StaticClass());
		Info.bHasBlueprintClasses = false;
		Info.bIsEditorOnly = false;
		Info.GetSpecificAssets().Add(DefinitionPath);
		Info.Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		AssetManagerSettings->PrimaryAssetTypesToScan.Add(Info);
		bChanged = true;
	}
	else
	{
		// The user already scans this type. Only add the asset when none of the entries would find it.
		if (!IsCoveredByEntry(*Existing, DefinitionPath, DefinitionPackage))
		{
			Existing->GetSpecificAssets().Add(DefinitionPath);
			bChanged = true;
		}

		if (Existing->Rules.CookRule == EPrimaryAssetCookRule::Unknown)
		{
			Existing->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		AssetManagerSettings->TryUpdateDefaultConfigFile();
		UE_LOG(LogSandwichSteamEditor, Log, TEXT("Added the Steam App Definition %s to Primary Asset Types to Scan (Always Cook) in DefaultGame.ini so packaged builds include it."), *DefinitionPath.ToString());
	}
	return bChanged;
}

bool FSteamCookHelper::IsAppDefinitionCooked()
{
	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	const UAssetManagerSettings* AssetManagerSettings = GetDefault<UAssetManagerSettings>();
	if (!ToolSettings || ToolSettings->AppDefinition.IsNull() || !AssetManagerSettings)
	{
		return false;
	}

	const FSoftObjectPath DefinitionPath = ToolSettings->AppDefinition.ToSoftObjectPath();
	const FString TypeName = USteamAppDefinition::GetAssetType().ToString();

	for (const FPrimaryAssetTypeInfo& Info : AssetManagerSettings->PrimaryAssetTypesToScan)
	{
		if (Info.PrimaryAssetType.ToString() == TypeName
			&& Info.Rules.CookRule != EPrimaryAssetCookRule::NeverCook
			&& IsCoveredByEntry(Info, DefinitionPath, DefinitionPath.GetLongPackageName()))
		{
			return true;
		}
	}
	return false;
}
