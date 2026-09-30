// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Debug/SteamConsoleCommands.h"

#if SANDWICHSTEAM_WITH_DEBUG

#include "Core/SteamCoreSubsystem.h"
#include "Debug/SteamDebugPanel.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace SandwichSteam::Debug
{
	namespace
	{
		TArray<TUniquePtr<FAutoConsoleCommandWithWorldArgsAndOutputDevice>>& GetCommands()
		{
			static TArray<TUniquePtr<FAutoConsoleCommandWithWorldArgsAndOutputDevice>> Commands;
			return Commands;
		}

		USteamCoreSubsystem* FindCore(UWorld* World, FOutputDevice& Output)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			USteamCoreSubsystem* Core = GameInstance ? GameInstance->GetSubsystem<USteamCoreSubsystem>() : nullptr;
			if (!Core)
			{
				Output.Log(TEXT("No Steam core subsystem for this world. Run the command in a game (Standalone or PIE console), not the editor console without PIE."));
			}
			return Core;
		}

		void DumpCore(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
		{
			if (const USteamCoreSubsystem* Core = FindCore(World, Output))
			{
				Output.Log(*Core->BuildDebugString());
			}
		}

		void ThreadCheck(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
		{
			if (USteamCoreSubsystem* Core = FindCore(World, Output))
			{
				Core->RunThreadCheck();
				Output.Log(TEXT("Steam.Core.ThreadCheck started. The result is written to the log (LogSandwichSteam)."));
			}
		}

		void ShowDebugPanel(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& /*Output*/)
		{
			ToggleOverlay(World);
		}
	}

	void RegisterCommand(const TCHAR* Name, const TCHAR* Help, const FConsoleCommandWithWorldArgsAndOutputDeviceDelegate& Delegate)
	{
		GetCommands().Add(MakeUnique<FAutoConsoleCommandWithWorldArgsAndOutputDevice>(Name, Help, Delegate));
	}

	void RegisterCoreCommands()
	{
		RegisterCommand(TEXT("Steam.Core.Dump"),
			TEXT("Prints the Steam state, owner game instance, AppId, SDK macros, dispatcher counters and pending join intent."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpCore));

		RegisterCommand(TEXT("Steam.Core.ThreadCheck"),
			TEXT("Sends a harmless Steam request and logs which thread the raw Steam callback ran on."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ThreadCheck));

		RegisterCommand(TEXT("Steam.Debug.Show"),
			TEXT("Shows or hides the live Steam debug panel over the game (Standalone or PIE). The panel lists a section per installed feature module."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ShowDebugPanel));

		RegisterCoreSections();
	}

	void UnregisterCommands()
	{
		HideOverlay();
		UnregisterCoreSections();
		GetCommands().Reset();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG
