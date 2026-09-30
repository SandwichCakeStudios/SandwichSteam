// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Utility/SteamUtilityLibrary.h"
#include "Features/Utility/SteamUtilitySubsystem.h"

int32 USteamUtilityLibrary::GetSteamAppId(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility ? Utility->GetAppId() : 0;
}

FString USteamUtilityLibrary::GetSteamIpCountry(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility ? Utility->GetIpCountry() : FString();
}

FString USteamUtilityLibrary::GetSteamUiLanguage(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility ? Utility->GetSteamUiLanguage() : FString();
}

FString USteamUtilityLibrary::GetSteamGameLanguage(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility ? Utility->GetCurrentGameLanguage() : FString();
}

FDateTime USteamUtilityLibrary::GetSteamServerTime(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility ? Utility->GetServerRealTime() : FDateTime::FromUnixTimestamp(0);
}

int32 USteamUtilityLibrary::GetSteamSecondsSinceAppActive(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility ? Utility->GetSecondsSinceAppActive() : 0;
}

bool USteamUtilityLibrary::IsRunningOnSteamDeck(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility && Utility->IsRunningOnSteamDeck();
}

bool USteamUtilityLibrary::IsSteamBigPictureMode(const UObject* WorldContextObject)
{
	const USteamUtilitySubsystem* Utility = USteamUtilitySubsystem::Get(WorldContextObject);
	return Utility && Utility->IsBigPictureMode();
}
