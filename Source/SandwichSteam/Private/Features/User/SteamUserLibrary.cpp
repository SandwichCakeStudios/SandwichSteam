// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/User/SteamUserLibrary.h"
#include "Features/User/SteamUserSubsystem.h"

FString USteamUserLibrary::GetSteamPersonaName(const UObject* WorldContextObject)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User ? User->GetPersonaName() : FString();
}

int32 USteamUserLibrary::GetSteamLevel(const UObject* WorldContextObject)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User ? User->GetSteamLevel() : 0;
}

bool USteamUserLibrary::IsSteamLoggedOn(const UObject* WorldContextObject)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User && User->IsLoggedOn();
}

bool USteamUserLibrary::IsSteamSubscribed(const UObject* WorldContextObject)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User && User->IsSubscribed();
}

bool USteamUserLibrary::IsSteamSubscribedApp(const UObject* WorldContextObject, int32 AppId)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User && User->IsSubscribedApp(AppId);
}

bool USteamUserLibrary::IsSteamFamilySharedLicense(const UObject* WorldContextObject)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User && User->IsFamilySharedLicense();
}

FSteamId USteamUserLibrary::GetSteamAppOwner(const UObject* WorldContextObject)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User ? User->GetAppOwner() : FSteamId();
}

FDateTime USteamUserLibrary::GetSteamEarliestPurchaseTime(const UObject* WorldContextObject, int32 AppId)
{
	const USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User ? User->GetEarliestPurchaseTime(AppId) : FDateTime::FromUnixTimestamp(0);
}

UTexture2D* USteamUserLibrary::GetCachedSteamAvatar(const UObject* WorldContextObject, FSteamId UserId, ESteamAvatarSize Size)
{
	USteamUserSubsystem* User = USteamUserSubsystem::Get(WorldContextObject);
	return User ? User->GetCachedAvatar(UserId, Size) : nullptr;
}
