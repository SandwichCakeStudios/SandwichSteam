// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamDLCLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamNameWarnings.h"
#include "SteamDLCSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "DlcLibNotAvailable", "The DLC feature is not available in this game instance."));
	}
}

bool USteamDLCLibrary::IsSteamDLCOwned(const UObject* WorldContextObject, FGameplayTag DLC)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem && Subsystem->IsOwned(DLC);
}

bool USteamDLCLibrary::IsSteamDLCInstalled(const UObject* WorldContextObject, FGameplayTag DLC)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem && Subsystem->IsInstalled(DLC);
}

bool USteamDLCLibrary::IsSteamDLCAppOwned(const UObject* WorldContextObject, int32 AppId)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem && Subsystem->IsOwnedApp(AppId);
}

TArray<FSteamDLCInfo> USteamDLCLibrary::ListSteamDLC(const UObject* WorldContextObject)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->ListDLC() : TArray<FSteamDLCInfo>();
}

FSteamResult USteamDLCLibrary::InstallSteamDLC(const UObject* WorldContextObject, FGameplayTag DLC)
{
	USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->Install(DLC) : NotAvailable();
}

FSteamResult USteamDLCLibrary::UninstallSteamDLC(const UObject* WorldContextObject, FGameplayTag DLC)
{
	USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->Uninstall(DLC) : NotAvailable();
}

FSteamDLCProgress USteamDLCLibrary::GetSteamDLCProgress(const UObject* WorldContextObject, FGameplayTag DLC)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetDownloadProgress(DLC) : FSteamDLCProgress();
}

float USteamDLCLibrary::GetSteamDLCProgressFraction(const FSteamDLCProgress& Progress)
{
	return Progress.GetFraction();
}

FSteamResult USteamDLCLibrary::OpenSteamDLCStorePage(const UObject* WorldContextObject, FGameplayTag DLC)
{
	USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->OpenStorePage(DLC) : NotAvailable();
}

bool USteamDLCLibrary::IsSteamDLCInstalledByAppId(const UObject* WorldContextObject, int32 AppId)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem && Subsystem->IsInstalledApp(AppId);
}

FSteamResult USteamDLCLibrary::InstallSteamDLCByAppId(const UObject* WorldContextObject, int32 AppId)
{
	USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? SandwichSteam::WarnOnceIfInvalid(TEXT("DLC"), FString::FromInt(AppId), Subsystem->InstallApp(AppId)) : NotAvailable();
}

FSteamResult USteamDLCLibrary::UninstallSteamDLCByAppId(const UObject* WorldContextObject, int32 AppId)
{
	USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? SandwichSteam::WarnOnceIfInvalid(TEXT("DLC"), FString::FromInt(AppId), Subsystem->UninstallApp(AppId)) : NotAvailable();
}

FSteamDLCProgress USteamDLCLibrary::GetSteamDLCProgressByAppId(const UObject* WorldContextObject, int32 AppId)
{
	const USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? Subsystem->GetDownloadProgressApp(AppId) : FSteamDLCProgress();
}

FSteamResult USteamDLCLibrary::OpenSteamDLCStorePageByAppId(const UObject* WorldContextObject, int32 AppId)
{
	USteamDLCSubsystem* Subsystem = USteamDLCSubsystem::Get(WorldContextObject);
	return Subsystem ? SandwichSteam::WarnOnceIfInvalid(TEXT("DLC"), FString::FromInt(AppId), Subsystem->OpenStorePageApp(AppId)) : NotAvailable();
}
