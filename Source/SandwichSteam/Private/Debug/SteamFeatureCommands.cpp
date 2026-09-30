// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Debug/SteamConsoleCommands.h"

#if SANDWICHSTEAM_WITH_DEBUG

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/Overlay/SteamOverlaySubsystem.h"
#include "Features/User/SteamUserSubsystem.h"
#include "Features/Utility/SteamUtilitySubsystem.h"

namespace SandwichSteam::Debug
{
	namespace
	{
		template <typename TSubsystem>
		TSubsystem* FindFeature(UWorld* World, FOutputDevice& Output)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			TSubsystem* Feature = GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
			if (!Feature)
			{
				Output.Log(TEXT("Feature is not available for this world: disabled in the settings, or not running in a game (Standalone or PIE console)."));
			}
			return Feature;
		}

		void DumpUser(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
		{
			if (const USteamUserSubsystem* User = FindFeature<USteamUserSubsystem>(World, Output))
			{
				Output.Log(*User->BuildDebugString());
			}
		}

		void DumpUtility(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
		{
			if (const USteamUtilitySubsystem* Utility = FindFeature<USteamUtilitySubsystem>(World, Output))
			{
				Output.Log(*Utility->BuildDebugString());
			}
		}

		void DumpOverlay(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
		{
			if (const USteamOverlaySubsystem* Overlay = FindFeature<USteamOverlaySubsystem>(World, Output))
			{
				Output.Log(*Overlay->BuildDebugString());
			}
		}

		void OpenOverlay(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
		{
			USteamOverlaySubsystem* Overlay = FindFeature<USteamOverlaySubsystem>(World, Output);
			if (!Overlay)
			{
				return;
			}

			const UEnum* DialogEnum = StaticEnum<ESteamOverlayDialog>();
			const int64 DialogValue = Args.IsEmpty() ? INDEX_NONE : DialogEnum->GetValueByNameString(Args[0]);
			if (DialogValue == INDEX_NONE)
			{
				FString Names;
				for (int32 Index = 0; Index < DialogEnum->NumEnums() - 1; ++Index)
				{
					Names += DialogEnum->GetNameStringByIndex(Index) + TEXT(" ");
				}
				Output.Logf(TEXT("Usage: Steam.Overlay.Open <Dialog> [SteamId | AppId | Url]. Dialogs: %s"), *Names);
				return;
			}

			const ESteamOverlayDialog Dialog = static_cast<ESteamOverlayDialog>(DialogValue);
			const FString Argument = Args.IsValidIndex(1) ? Args[1] : FString();

			FSteamResult Result;
			switch (Dialog)
			{
			case ESteamOverlayDialog::Store:
				Result = Overlay->OpenStore(Argument.IsEmpty() ? 0 : FCString::Atoi(*Argument), ESteamOverlayStoreFlag::None);
				break;
			case ESteamOverlayDialog::WebPage:
				Result = Overlay->OpenWebPage(Argument, false);
				break;
			case ESteamOverlayDialog::InviteDialog:
			case ESteamOverlayDialog::UserProfile:
			case ESteamOverlayDialog::UserChat:
			case ESteamOverlayDialog::AddFriend:
				{
					FSteamId Target;
					FSteamId::FromString(Argument, Target); // Stays invalid on bad input; the subsystem reports it.
					Result = Overlay->OpenTargetDialog(Dialog, Target);
				}
				break;
			default:
				Result = Overlay->OpenDialog(Dialog);
				break;
			}

			Output.Logf(TEXT("Steam.Overlay.Open %s: %s"), *DialogEnum->GetNameStringByValue(DialogValue),
				Result.IsSuccess() ? TEXT("ok") : *Result.Message.ToString());
		}
	}

	void RegisterFeatureCommands()
	{
		RegisterCommand(TEXT("Steam.User.Dump"),
			TEXT("Prints the local Steam user, login state, ownership, avatar cache and Web API ticket support."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpUser));

		RegisterCommand(TEXT("Steam.Utility.Dump"),
			TEXT("Prints the Steam App ID, country, languages, server time and Steam Deck / Big Picture flags."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpUtility));

		RegisterCommand(TEXT("Steam.Overlay.Dump"),
			TEXT("Prints whether the Steam overlay is enabled and open, and the auto pause state."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpOverlay));

		RegisterCommand(TEXT("Steam.Overlay.Open"),
			TEXT("Opens a Steam overlay page: Steam.Overlay.Open <Friends|Community|Players|Settings|OfficialGameGroup|Stats|Achievements|Store|WebPage|UserProfile|UserChat|AddFriend|InviteDialog> [SteamId | AppId | Url]."),
			FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&OpenOverlay));
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG
