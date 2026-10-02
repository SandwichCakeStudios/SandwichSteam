// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamInput.h"
#include "Core/SteamEngineCompat.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Debug/SteamDebugSection.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Misc/Parse.h"
#include "SteamInputKeys.h"
#include "SteamInputSubsystem.h"
#include "UObject/UObjectGlobals.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void LogResult(FOutputDevice& Output, const TCHAR* Command, const FSteamResult& Result, const TCHAR* Done)
	{
		Output.Logf(TEXT("%s: %s"), Command, Result.IsSuccess() ? Done : *Result.Message.ToString());
	}

	FGameplayTag ParseTag(const FString& Text)
	{
		return FGameplayTag::RequestGameplayTag(FName(*Text), /*ErrorIfNotFound*/ false);
	}

	void DumpInput(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamInputSubsystem* Input = FSteamDebugCommandSet::FindFeature<USteamInputSubsystem>(World, Output))
		{
			Output.Log(*Input->BuildDebugString());
		}
	}

	/** Steam.Input.Set <ActionSetTag> */
	void SetActionSet(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamInputSubsystem* Input = FSteamDebugCommandSet::FindFeature<USteamInputSubsystem>(World, Output);
		if (!Input)
		{
			return;
		}

		const FGameplayTag Tag = Args.IsEmpty() ? FGameplayTag() : ParseTag(Args[0]);
		if (!Tag.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Input.Set <Steam.Input.ActionSet.Tag>"));
			return;
		}
		LogResult(Output, TEXT("Steam.Input.Set"), Input->SetActionSet(Tag), TEXT("action set switched"));
	}

	/** Steam.Input.Layer <LayerTag> <0|1> */
	void SetLayer(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamInputSubsystem* Input = FSteamDebugCommandSet::FindFeature<USteamInputSubsystem>(World, Output);
		if (!Input)
		{
			return;
		}

		const FGameplayTag Tag = Args.IsEmpty() ? FGameplayTag() : ParseTag(Args[0]);
		if (!Tag.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Input.Layer <Steam.Input.ActionSet.Tag> <0|1>"));
			return;
		}

		const bool bOn = Args.Num() < 2 || FCString::Atoi(*Args[1]) != 0;
		LogResult(Output, TEXT("Steam.Input.Layer"), bOn ? Input->ActivateLayer(Tag) : Input->DeactivateLayer(Tag), bOn ? TEXT("layer on") : TEXT("layer off"));
	}

	/** Steam.Input.Rumble [Slot] [Left] [Right] [Seconds]  (Slot -1 = all; no arguments = a 0.5 s pulse on every controller) */
	void Rumble(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamInputSubsystem* Input = FSteamDebugCommandSet::FindFeature<USteamInputSubsystem>(World, Output);
		if (!Input)
		{
			return;
		}

		const int32 Slot = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
		const float Left = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.6f;
		const float Right = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.6f;
		const float Seconds = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.5f;
		LogResult(Output, TEXT("Steam.Input.Rumble"), Input->TriggerVibration(Slot < 0 ? INDEX_NONE : Slot, Left, Right, 0.0f, 0.0f, Seconds), TEXT("vibration started"));
	}

	/** Steam.Input.Panel [Slot] */
	void ShowPanel(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamInputSubsystem* Input = FSteamDebugCommandSet::FindFeature<USteamInputSubsystem>(World, Output);
		if (!Input)
		{
			return;
		}

		const int32 Slot = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
		LogResult(Output, TEXT("Steam.Input.Panel"), Input->ShowBindingPanel(Slot < 0 ? INDEX_NONE : Slot), TEXT("binding panel requested"));
	}

	/** Steam.Input.Glyph <Action> [Slot] */
	void Glyph(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamInputSubsystem* Input = FSteamDebugCommandSet::FindFeature<USteamInputSubsystem>(World, Output);
		if (!Input)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Input.Glyph <Action> [Slot]"));
			return;
		}

		FSteamInputGlyph Result;
		const int32 Slot = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1;
		const FSteamResult Status = Input->GetGlyphForAction(FName(*Args[0]), Result, Slot < 0 ? INDEX_NONE : Slot);
		if (Status.IsSuccess())
		{
			Output.Logf(TEXT("Steam.Input.Glyph: %s -> \"%s\", texture %s"), *Args[0], *Result.Label.ToString(), Result.Texture ? *Result.Texture->GetName() : TEXT("none"));
		}
		else
		{
			Output.Logf(TEXT("Steam.Input.Glyph: %s"), *Status.Message.ToString());
		}
	}

	const FName DebugSectionId(TEXT("Input"));

	FString ReportInput(UWorld* World)
	{
		const USteamInputSubsystem* Input = SandwichSteam::Debug::FindFeatureSubsystem<USteamInputSubsystem>(World);
		return Input ? Input->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamInputModule::StartupModule()
{
	// The keys are needed while editing (key picker) and before any game instance exists. Load order: register right away when the engine is up.
	if (GEngine)
	{
		RegisterKeysFromSettings();
	}
	else
	{
		PostEngineInitHandle = SandwichSteam::Compat::OnPostEngineInit().AddRaw(this, &FSandwichSteamInputModule::RegisterKeysFromSettings);
	}

#if WITH_EDITOR
	// A new or renamed action in the App Definition gets its key at once (keys cannot be removed again until the editor restarts).
	ObjectChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([](UObject* Object, FPropertyChangedEvent& Event)
	{
		if (Event.ChangeType != EPropertyChangeType::Interactive)
		{
			if (const USteamAppDefinition* Definition = Cast<USteamAppDefinition>(Object))
			{
				SandwichSteam::Input::RegisterKeys(*Definition);
			}
		}
	});
	DefinitionChangedHandle = USteamToolSettings::OnAppDefinitionChanged().AddRaw(this, &FSandwichSteamInputModule::RegisterKeysFromSettings);
#endif

#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamInput", "DebugTitle", "Input");
	Section.Order = 110;
	Section.BuildReport = &ReportInput;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Input.Dump"),
		TEXT("Prints the Steam Input state: controllers, action set, layers, the actions read each frame, counters."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpInput));

	Commands.Add(TEXT("Steam.Input.Set"),
		TEXT("Switches the action set: Steam.Input.Set <Steam.Input.ActionSet.Tag>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetActionSet));

	Commands.Add(TEXT("Steam.Input.Layer"),
		TEXT("Turns an action set layer on or off: Steam.Input.Layer <Tag> <0|1>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetLayer));

	Commands.Add(TEXT("Steam.Input.Rumble"),
		TEXT("Rumbles a controller: Steam.Input.Rumble [Slot] [Left] [Right] [Seconds]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&Rumble));

	Commands.Add(TEXT("Steam.Input.Panel"),
		TEXT("Opens Steam's binding panel: Steam.Input.Panel [Slot]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ShowPanel));

	Commands.Add(TEXT("Steam.Input.Glyph"),
		TEXT("Prints the glyph of an action: Steam.Input.Glyph <Action> [Slot]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&Glyph));
#endif
}

void FSandwichSteamInputModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		SandwichSteam::Compat::OnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectChangedHandle);
	ObjectChangedHandle.Reset();
	USteamToolSettings::OnAppDefinitionChanged().Remove(DefinitionChangedHandle);
	DefinitionChangedHandle.Reset();
#endif

#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

void FSandwichSteamInputModule::RegisterKeysFromSettings()
{
	if (IsRunningCommandlet() || IsRunningDedicatedServer())
	{
		return;
	}

	const USteamToolSettings* Settings = USteamToolSettings::Get();
	const USteamAppDefinition* Definition = Settings ? Settings->LoadAppDefinition() : nullptr;
	if (!Definition)
	{
		return;
	}

	const int32 Added = SandwichSteam::Input::RegisterKeys(*Definition);
	if (Added > 0)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: registered %d key(s) from %s."), Added, *Definition->GetName());
	}
}

IMPLEMENT_MODULE(FSandwichSteamInputModule, SandwichSteamInput)
