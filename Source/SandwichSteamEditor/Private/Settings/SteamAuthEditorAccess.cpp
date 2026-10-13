// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SteamAuthEditorAccess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Settings/SteamIniWriter.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* AuthModuleName = TEXT("SandwichSteamAuth");
	const TCHAR* SettingsClassPath = TEXT("/Script/SandwichSteamAuth.SteamAuthSettings");
	const FName EnabledPropertyName(TEXT("bEnableSteamAuthentication"));
	const FName PolicyPropertyName(TEXT("FailurePolicy"));
	const TCHAR* CustomPolicyName = TEXT("ESteamAuthFailurePolicy::Custom");
	const TCHAR* KickPolicyName = TEXT("ESteamAuthFailurePolicy::Kick");

	UObject* FindSettings()
	{
		if (!FModuleManager::Get().IsModuleLoaded(AuthModuleName))
		{
			return nullptr;
		}
		const UClass* Class = FindObject<UClass>(nullptr, SettingsClassPath);
		return Class ? Class->GetDefaultObject() : nullptr;
	}

	/** Sets one property the way the details panel would (listeners see the edit), then saves the DefaultConfig object. */
	void EditAndSave(UObject* Settings, FProperty* Property, TFunctionRef<void()> Assign)
	{
		Settings->PreEditChange(Property);
		Assign();
		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Settings->PostEditChangeProperty(Event);
		Settings->TryUpdateDefaultConfigFile();
	}
}

bool SandwichSteam::Editor::Auth::IsModuleInstalled()
{
	return FModuleManager::Get().IsModuleLoaded(AuthModuleName);
}

bool SandwichSteam::Editor::Auth::IsEnabled()
{
	const UObject* Settings = FindSettings();
	const FBoolProperty* Property = Settings ? FindFProperty<FBoolProperty>(Settings->GetClass(), EnabledPropertyName) : nullptr;
	return Property && Property->GetPropertyValue_InContainer(Settings);
}

void SandwichSteam::Editor::Auth::SetEnabled(bool bEnabled)
{
	UObject* Settings = FindSettings();
	FBoolProperty* Property = Settings ? FindFProperty<FBoolProperty>(Settings->GetClass(), EnabledPropertyName) : nullptr;
	if (Property)
	{
		EditAndSave(Settings, Property, [Settings, Property, bEnabled]() { Property->SetPropertyValue_InContainer(Settings, bEnabled); });
	}
}

bool SandwichSteam::Editor::Auth::IsCustomPolicy()
{
	const UObject* Settings = FindSettings();
	const FEnumProperty* Property = Settings ? FindFProperty<FEnumProperty>(Settings->GetClass(), PolicyPropertyName) : nullptr;
	if (!Property)
	{
		return false;
	}
	const int64 Value = Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Settings));
	return Value == Property->GetEnum()->GetValueByNameString(CustomPolicyName);
}

void SandwichSteam::Editor::Auth::SetCustomPolicy(bool bCustom)
{
	UObject* Settings = FindSettings();
	FEnumProperty* Property = Settings ? FindFProperty<FEnumProperty>(Settings->GetClass(), PolicyPropertyName) : nullptr;
	if (!Property)
	{
		return;
	}

	const int64 Value = Property->GetEnum()->GetValueByNameString(bCustom ? CustomPolicyName : KickPolicyName);
	if (Value == INDEX_NONE)
	{
		return;
	}
	EditAndSave(Settings, Property, [Settings, Property, Value]()
	{
		Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Settings), Value);
	});
}

bool SandwichSteam::Editor::Auth::HasIniLine()
{
	FString Ini;
	const FString IniPath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
	if (!FPaths::FileExists(IniPath) || !FFileHelper::LoadFileToString(Ini, *IniPath))
	{
		return false;
	}

	const TArray<FSteamIniChange> Changes = FSteamIniWriter::Diff(Ini, FSteamIniWriter::BuildSteamAuthEntries(true));
	return Changes.Num() == 1 && Changes[0].Change == ESteamIniChange::Unchanged;
}
