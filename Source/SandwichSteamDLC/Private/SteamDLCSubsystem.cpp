// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamDLCSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/Overlay/SteamOverlaySubsystem.h"
#include "SteamDLCBackend.h"

USteamDLCSubsystem* USteamDLCSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamDLCSubsystem>() : nullptr;
}

USteamDLCSubsystem::USteamDLCSubsystem() = default;
USteamDLCSubsystem::~USteamDLCSubsystem() = default;

FGameplayTag USteamDLCSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_DLC;
}

bool USteamDLCSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	Definition = ToolSettings ? ToolSettings->LoadAppDefinition() : nullptr;
	if (!Definition)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam DLC: no App Definition assigned in the Sandwich Steam settings. DLC work by App ID only."));
	}

	Backend = MakeShared<FSteamDLCBackend>(this, Dispatcher.ToSharedRef());
	return true;
#else
	return false;
#endif
}

void USteamDLCSubsystem::ShutdownFeature()
{
	Backend.Reset();
	Definition = nullptr;
}

int32 USteamDLCSubsystem::ResolveAppId(const FGameplayTag& DlcTag) const
{
	const FSteamDLCDef* Def = Definition ? Definition->FindDLC(DlcTag) : nullptr;
	return Def ? Def->AppId : 0;
}

FGameplayTag USteamDLCSubsystem::FindTag(int32 AppId) const
{
	const FSteamDLCDef* Def = Definition ? Definition->FindDLCByAppId(AppId) : nullptr;
	return Def ? Def->Tag : FGameplayTag();
}

FSteamResult USteamDLCSubsystem::ValidateAppId(int32 AppId) const
{
	if (AppId <= 0)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "DlcBadAppId", "The DLC is unknown: the tag is not a DLC row of the Steam App Definition, or the App ID is not valid."));
	}
	return FSteamResult::Success();
}

bool USteamDLCSubsystem::IsOwnedApp(int32 AppId) const
{
	return Backend.IsValid() && AppId > 0 && Backend->IsOwned(static_cast<uint32>(AppId));
}

bool USteamDLCSubsystem::IsOwned(const FGameplayTag& DlcTag) const
{
	return IsOwnedApp(ResolveAppId(DlcTag));
}

bool USteamDLCSubsystem::IsInstalledApp(int32 AppId) const
{
	return Backend.IsValid() && AppId > 0 && Backend->IsInstalled(static_cast<uint32>(AppId));
}

bool USteamDLCSubsystem::IsInstalled(const FGameplayTag& DlcTag) const
{
	return IsInstalledApp(ResolveAppId(DlcTag));
}

TArray<FSteamDLCInfo> USteamDLCSubsystem::ListDLC() const
{
	TArray<FSteamDLCInfo> List;
	if (Backend.IsValid())
	{
		Backend->ListDLC(List);
		for (FSteamDLCInfo& Info : List)
		{
			Info.Tag = FindTag(Info.AppId);
		}
	}
	return List;
}

FSteamResult USteamDLCSubsystem::InstallApp(int32 AppId)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = ValidateAppId(AppId);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!Backend->IsOwned(static_cast<uint32>(AppId)))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotOwner, NSLOCTEXT("SandwichSteam", "DlcNotOwned", "The local user does not own this DLC."));
	}

	Backend->Install(static_cast<uint32>(AppId));
	return FSteamResult::Success();
}

FSteamResult USteamDLCSubsystem::Install(const FGameplayTag& DlcTag)
{
	return InstallApp(ResolveAppId(DlcTag));
}

FSteamResult USteamDLCSubsystem::UninstallApp(int32 AppId)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = ValidateAppId(AppId);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	Backend->Uninstall(static_cast<uint32>(AppId));
	return FSteamResult::Success();
}

FSteamResult USteamDLCSubsystem::Uninstall(const FGameplayTag& DlcTag)
{
	return UninstallApp(ResolveAppId(DlcTag));
}

FSteamDLCProgress USteamDLCSubsystem::GetDownloadProgressApp(int32 AppId) const
{
	return (Backend.IsValid() && AppId > 0) ? Backend->GetProgress(static_cast<uint32>(AppId)) : FSteamDLCProgress();
}

FSteamDLCProgress USteamDLCSubsystem::GetDownloadProgress(const FGameplayTag& DlcTag) const
{
	return GetDownloadProgressApp(ResolveAppId(DlcTag));
}

FSteamResult USteamDLCSubsystem::OpenStorePage(const FGameplayTag& DlcTag)
{
	return OpenStorePageApp(ResolveAppId(DlcTag));
}

FSteamResult USteamDLCSubsystem::OpenStorePageApp(int32 AppId)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = ValidateAppId(AppId);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	USteamOverlaySubsystem* Overlay = GameInstance ? GameInstance->GetSubsystem<USteamOverlaySubsystem>() : nullptr;
	if (!Overlay)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "DlcNoOverlay", "The Overlay feature is disabled, so the store page cannot be opened."));
	}

	return Overlay->OpenStore(AppId, ESteamOverlayStoreFlag::AddToCartAndShow);
}

void USteamDLCSubsystem::HandleDlcInstalled(int32 AppId)
{
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam DLC: %d finished installing."), AppId);
	OnDLCInstalled.Broadcast(AppId, FindTag(AppId));
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamDLCSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.DLC: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	Report += FString::Printf(TEXT("  Definition: %s (%d DLC rows)\n"), Definition ? *Definition->GetName() : TEXT("none"), Definition ? Definition->DLC.Num() : 0);

	if (Definition)
	{
		for (const FSteamDLCDef& Def : Definition->DLC)
		{
			Report += FString::Printf(TEXT("  [%s] %d  owned %s, installed %s\n"), *Def.Tag.ToString(), Def.AppId,
				IsOwnedApp(Def.AppId) ? TEXT("yes") : TEXT("no"), IsInstalledApp(Def.AppId) ? TEXT("yes") : TEXT("no"));
		}
	}

	const TArray<FSteamDLCInfo> List = ListDLC();
	Report += FString::Printf(TEXT("  Steam lists %d DLC:\n"), List.Num());
	for (const FSteamDLCInfo& Info : List)
	{
		Report += FString::Printf(TEXT("    %d  %s  available %s, owned %s, installed %s%s\n"), Info.AppId, *Info.Name, Info.bAvailable ? TEXT("yes") : TEXT("no"),
			Info.bOwned ? TEXT("yes") : TEXT("no"), Info.bInstalled ? TEXT("yes") : TEXT("no"), Info.Tag.IsValid() ? TEXT("") : TEXT("  (no row in the App Definition)"));
	}

	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
