// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamCloudLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "SteamCloudSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "CloudLibNotAvailable", "The Cloud feature is not available in this game instance."));
	}
}

FSteamCloudSaveResult USteamCloudLibrary::SaveGameToSteamCloud(const UObject* WorldContextObject, const FString& Slot, USaveGame* SaveGame)
{
	if (USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject))
	{
		return Cloud->SaveGameToCloud(Slot, SaveGame);
	}

	FSteamCloudSaveResult Out;
	Out.Result = NotAvailable();
	return Out;
}

FSteamCloudLoadResult USteamCloudLibrary::LoadGameFromSteamCloud(const UObject* WorldContextObject, const FString& Slot, USaveGame*& SaveGame)
{
	SaveGame = nullptr;
	if (USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject))
	{
		return Cloud->LoadGameFromCloud(Slot, SaveGame);
	}

	FSteamCloudLoadResult Out;
	Out.Result = NotAvailable();
	return Out;
}

FSteamCloudLoadResult USteamCloudLibrary::ResolveSteamCloudConflict(const UObject* WorldContextObject, const FString& Slot, bool bUseCloud, USaveGame*& SaveGame)
{
	SaveGame = nullptr;
	if (USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject))
	{
		return Cloud->ResolveConflictToSaveGame(Slot, bUseCloud, SaveGame);
	}

	FSteamCloudLoadResult Out;
	Out.Result = NotAvailable();
	return Out;
}

FSteamResult USteamCloudLibrary::DeleteSteamCloudSlot(const UObject* WorldContextObject, const FString& Slot)
{
	USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject);
	return Cloud ? Cloud->DeleteSlot(Slot) : NotAvailable();
}

TArray<FSteamCloudSlotInfo> USteamCloudLibrary::ListSteamCloudSlots(const UObject* WorldContextObject)
{
	const USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject);
	return Cloud ? Cloud->ListCloudSlots() : TArray<FSteamCloudSlotInfo>();
}

bool USteamCloudLibrary::IsSteamCloudEnabled(const UObject* WorldContextObject)
{
	const USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject);
	return Cloud && Cloud->IsCloudEnabled();
}

bool USteamCloudLibrary::GetSteamCloudQuota(const UObject* WorldContextObject, FSteamCloudQuota& Quota)
{
	const USteamCloudSubsystem* Cloud = USteamCloudSubsystem::Get(WorldContextObject);
	Quota = FSteamCloudQuota();
	return Cloud && Cloud->GetQuota(Quota);
}
