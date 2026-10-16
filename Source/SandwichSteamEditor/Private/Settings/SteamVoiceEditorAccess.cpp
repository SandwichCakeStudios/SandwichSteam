// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SteamVoiceEditorAccess.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

const FName SandwichSteam::Editor::Voice::UseSteamPipelinePropertyName(TEXT("bUseSteamVoicePipeline"));

namespace
{
	const TCHAR* VoiceModuleName = TEXT("SandwichSteamVoice");
	const TCHAR* SettingsClassPath = TEXT("/Script/SandwichSteamVoice.SteamVoiceSettings");

	const UObject* FindSettings()
	{
		if (!FModuleManager::Get().IsModuleLoaded(VoiceModuleName))
		{
			return nullptr;
		}
		const UClass* Class = FindObject<UClass>(nullptr, SettingsClassPath);
		return Class ? Class->GetDefaultObject() : nullptr;
	}
}

bool SandwichSteam::Editor::Voice::IsModuleInstalled()
{
	return FModuleManager::Get().IsModuleLoaded(VoiceModuleName);
}

bool SandwichSteam::Editor::Voice::IsSettingsObject(const UObject* Object)
{
	return Object && Object == FindSettings();
}

bool SandwichSteam::Editor::Voice::UsesSteamPipeline()
{
	const UObject* Settings = FindSettings();
	const FBoolProperty* Property = Settings ? FindFProperty<FBoolProperty>(Settings->GetClass(), UseSteamPipelinePropertyName) : nullptr;
	return !Property || Property->GetPropertyValue_InContainer(Settings);
}
