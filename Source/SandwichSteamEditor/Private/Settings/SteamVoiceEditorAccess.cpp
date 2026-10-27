// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Settings/SteamVoiceEditorAccess.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

const FName SandwichSteam::Editor::Voice::UseSteamPipelinePropertyName(TEXT("bUseSteamVoicePipeline"));

namespace SandwichSteam::Editor::Voice
{
	namespace
	{
		const TCHAR* VoiceModuleName = TEXT("SandwichSteamVoice");
		const TCHAR* SettingsClassPath = TEXT("/Script/SandwichSteamVoice.SteamVoiceSettings");
		const FName ChannelTagMapPropertyName(TEXT("ChannelTagMap"));

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

TMap<FGameplayTag, int32> SandwichSteam::Editor::Voice::GetChannelTagMap()
{
	const UObject* Settings = FindSettings();
	const FMapProperty* Property = Settings ? FindFProperty<FMapProperty>(Settings->GetClass(), ChannelTagMapPropertyName) : nullptr;
	const FStructProperty* KeyProperty = Property ? CastField<FStructProperty>(Property->KeyProp) : nullptr;
	if (!KeyProperty || KeyProperty->Struct != FGameplayTag::StaticStruct() || !Property->ValueProp->IsA<FIntProperty>())
	{
		return TMap<FGameplayTag, int32>();
	}
	return *Property->ContainerPtrToValuePtr<TMap<FGameplayTag, int32>>(Settings);
}
