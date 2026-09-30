// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#if SANDWICHSTEAM_WITH_DEBUG

/**
 * Console commands owned by one feature module (non-Shipping builds only).
 * Keep one as a member of the module class, Add() in StartupModule and Reset() in ShutdownModule, so the commands
 * disappear together with the module that implements them.
 */
class FSteamDebugCommandSet
{
public:
	void Add(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Delegate)
	{
		Commands.Add(MakeUnique<FAutoConsoleCommandWithWorldArgsAndOutputDevice>(Name, Help, Delegate));
	}

	void Reset() { Commands.Reset(); }

	/** Finds a GameInstance subsystem for the console world, or logs a hint and returns nullptr. */
	template <typename TSubsystem>
	static TSubsystem* FindFeature(UWorld* World, FOutputDevice& Output)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		TSubsystem* Feature = GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
		if (!Feature)
		{
			Output.Log(TEXT("Feature is not available for this world: disabled in the settings, or not running in a game (Standalone or PIE console)."));
		}
		return Feature;
	}

private:
	TArray<TUniquePtr<FAutoConsoleCommandWithWorldArgsAndOutputDevice>> Commands;
};

#endif // SANDWICHSTEAM_WITH_DEBUG
