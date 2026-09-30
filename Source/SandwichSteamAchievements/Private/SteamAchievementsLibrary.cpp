// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAchievementsLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamNameWarnings.h"
#include "SteamAchievementsSubsystem.h"

namespace
{
	FSteamResult NoAchievements()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled,
			NSLOCTEXT("SandwichSteam", "AchNoSubsystem", "The Steam achievements feature is not available (disabled in the settings, or no game instance)."));
	}
}

FSteamResult USteamAchievementsLibrary::UnlockSteamAchievement(const UObject* WorldContextObject, FGameplayTag AchievementTag)
{
	USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	return Achievements ? Achievements->Unlock(AchievementTag) : NoAchievements();
}

FSteamResult USteamAchievementsLibrary::ClearSteamAchievement(const UObject* WorldContextObject, FGameplayTag AchievementTag)
{
	USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	return Achievements ? Achievements->Clear(AchievementTag) : NoAchievements();
}

bool USteamAchievementsLibrary::IsSteamAchievementUnlocked(const UObject* WorldContextObject, FGameplayTag AchievementTag)
{
	const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	bool bUnlocked = false;
	FDateTime UnlockTime;
	return Achievements && Achievements->GetState(AchievementTag, bUnlocked, UnlockTime).IsSuccess() && bUnlocked;
}

FSteamResult USteamAchievementsLibrary::GetSteamAchievementInfo(const UObject* WorldContextObject, FGameplayTag AchievementTag, FSteamAchievementInfo& Info)
{
	const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	if (!Achievements)
	{
		Info = FSteamAchievementInfo();
		return NoAchievements();
	}
	return Achievements->GetInfo(AchievementTag, Info);
}

TArray<FSteamAchievementInfo> USteamAchievementsLibrary::GetAllSteamAchievements(const UObject* WorldContextObject)
{
	TArray<FSteamAchievementInfo> Infos;
	if (const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject))
	{
		Achievements->GetAllInfo(Infos);
	}
	return Infos;
}

FSteamResult USteamAchievementsLibrary::IndicateSteamAchievementProgress(const UObject* WorldContextObject, FGameplayTag AchievementTag, int32 CurrentValue)
{
	USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	return Achievements ? Achievements->IndicateProgress(AchievementTag, CurrentValue) : NoAchievements();
}

bool USteamAchievementsLibrary::GetSteamAchievementGlobalPercent(const UObject* WorldContextObject, FGameplayTag AchievementTag, float& Percent)
{
	const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	Percent = 0.f;
	return Achievements && Achievements->GetGlobalPercent(AchievementTag, Percent);
}

FSteamResult USteamAchievementsLibrary::UnlockSteamAchievementByName(const UObject* WorldContextObject, FName AchievementName)
{
	USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	return Achievements ? SandwichSteam::WarnOnceIfInvalid(TEXT("achievement"), AchievementName.ToString(), Achievements->Unlock(AchievementName)) : NoAchievements();
}

FSteamResult USteamAchievementsLibrary::ClearSteamAchievementByName(const UObject* WorldContextObject, FName AchievementName)
{
	USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	return Achievements ? SandwichSteam::WarnOnceIfInvalid(TEXT("achievement"), AchievementName.ToString(), Achievements->Clear(AchievementName)) : NoAchievements();
}

bool USteamAchievementsLibrary::IsSteamAchievementUnlockedByName(const UObject* WorldContextObject, FName AchievementName)
{
	const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	bool bUnlocked = false;
	FDateTime UnlockTime;
	return Achievements
		&& SandwichSteam::WarnOnceIfInvalid(TEXT("achievement"), AchievementName.ToString(), Achievements->GetState(AchievementName, bUnlocked, UnlockTime)).IsSuccess()
		&& bUnlocked;
}

FSteamResult USteamAchievementsLibrary::GetSteamAchievementInfoByName(const UObject* WorldContextObject, FName AchievementName, FSteamAchievementInfo& Info)
{
	const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	if (!Achievements)
	{
		Info = FSteamAchievementInfo();
		return NoAchievements();
	}
	return SandwichSteam::WarnOnceIfInvalid(TEXT("achievement"), AchievementName.ToString(), Achievements->GetInfo(AchievementName, Info));
}

FSteamResult USteamAchievementsLibrary::IndicateSteamAchievementProgressByName(const UObject* WorldContextObject, FName AchievementName, int32 CurrentValue, int32 MaxValue)
{
	USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	return Achievements
		? SandwichSteam::WarnOnceIfInvalid(TEXT("achievement"), AchievementName.ToString(), Achievements->IndicateProgress(AchievementName, CurrentValue, MaxValue))
		: NoAchievements();
}

bool USteamAchievementsLibrary::GetSteamAchievementGlobalPercentByName(const UObject* WorldContextObject, FName AchievementName, float& Percent)
{
	const USteamAchievementsSubsystem* Achievements = USteamAchievementsSubsystem::Get(WorldContextObject);
	Percent = 0.f;
	return Achievements && Achievements->GetGlobalPercent(AchievementName, Percent);
}
