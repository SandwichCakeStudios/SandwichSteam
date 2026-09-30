// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamStatsLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamNameWarnings.h"
#include "SteamStatsSubsystem.h"

namespace
{
	FSteamResult NoStats()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled,
			NSLOCTEXT("SandwichSteam", "StatsNoSubsystem", "The Steam stats feature is not available (disabled in the settings, or no game instance)."));
	}
}

bool USteamStatsLibrary::AreSteamStatsReady(const UObject* WorldContextObject)
{
	const USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats && Stats->IsFeatureActive() && Stats->AreStatsReady();
}

FSteamResult USteamStatsLibrary::GetSteamStatInt(const UObject* WorldContextObject, FGameplayTag StatTag, int32& Value)
{
	const USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	Value = 0;
	return Stats ? Stats->GetInt(StatTag, Value) : NoStats();
}

FSteamResult USteamStatsLibrary::GetSteamStatFloat(const UObject* WorldContextObject, FGameplayTag StatTag, float& Value)
{
	const USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	Value = 0.f;
	return Stats ? Stats->GetFloat(StatTag, Value) : NoStats();
}

FSteamResult USteamStatsLibrary::SetSteamStatInt(const UObject* WorldContextObject, FGameplayTag StatTag, int32 Value)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? Stats->SetInt(StatTag, Value) : NoStats();
}

FSteamResult USteamStatsLibrary::SetSteamStatFloat(const UObject* WorldContextObject, FGameplayTag StatTag, float Value)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? Stats->SetFloat(StatTag, Value) : NoStats();
}

FSteamResult USteamStatsLibrary::AddSteamStatInt(const UObject* WorldContextObject, FGameplayTag StatTag, int32 Delta)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? Stats->AddInt(StatTag, Delta) : NoStats();
}

FSteamResult USteamStatsLibrary::AddSteamStatFloat(const UObject* WorldContextObject, FGameplayTag StatTag, float Delta)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? Stats->AddFloat(StatTag, Delta) : NoStats();
}

FSteamResult USteamStatsLibrary::UpdateSteamAvgRateStat(const UObject* WorldContextObject, FGameplayTag StatTag, float Count, float SessionSeconds)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? Stats->UpdateAvgRate(StatTag, Count, SessionSeconds) : NoStats();
}

FSteamResult USteamStatsLibrary::StoreSteamStatsNow(const UObject* WorldContextObject)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? Stats->StoreStatsNow() : NoStats();
}

FSteamResult USteamStatsLibrary::GetSteamStatIntByName(const UObject* WorldContextObject, FName StatName, int32& Value)
{
	const USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	Value = 0;
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->GetInt(StatName, Value)) : NoStats();
}

FSteamResult USteamStatsLibrary::GetSteamStatFloatByName(const UObject* WorldContextObject, FName StatName, float& Value)
{
	const USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	Value = 0.f;
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->GetFloat(StatName, Value)) : NoStats();
}

FSteamResult USteamStatsLibrary::SetSteamStatIntByName(const UObject* WorldContextObject, FName StatName, int32 Value)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->SetInt(StatName, Value)) : NoStats();
}

FSteamResult USteamStatsLibrary::SetSteamStatFloatByName(const UObject* WorldContextObject, FName StatName, float Value)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->SetFloat(StatName, Value)) : NoStats();
}

FSteamResult USteamStatsLibrary::AddSteamStatIntByName(const UObject* WorldContextObject, FName StatName, int32 Delta)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->AddInt(StatName, Delta)) : NoStats();
}

FSteamResult USteamStatsLibrary::AddSteamStatFloatByName(const UObject* WorldContextObject, FName StatName, float Delta)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->AddFloat(StatName, Delta)) : NoStats();
}

FSteamResult USteamStatsLibrary::UpdateSteamAvgRateStatByName(const UObject* WorldContextObject, FName StatName, float Count, float SessionSeconds)
{
	USteamStatsSubsystem* Stats = USteamStatsSubsystem::Get(WorldContextObject);
	return Stats ? SandwichSteam::WarnOnceIfInvalid(TEXT("stat"), StatName.ToString(), Stats->UpdateAvgRate(StatName, Count, SessionSeconds)) : NoStats();
}
