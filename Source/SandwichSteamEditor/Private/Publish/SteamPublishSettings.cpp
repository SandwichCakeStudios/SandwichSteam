// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishSettings.h"
#include "Core/SteamToolSettings.h"

USteamPublishSettings::USteamPublishSettings()
{
	FSteamPublishBranch Beta;
	Beta.Name = TEXT("beta");
	Branches.Add(Beta);
}

const USteamPublishSettings* USteamPublishSettings::Get()
{
	return GetDefault<USteamPublishSettings>();
}

int32 USteamPublishSettings::GetAppId() const
{
	const USteamToolSettings* Runtime = USteamToolSettings::Get();
	return Runtime ? Runtime->SteamAppId : 0;
}

FString USteamPublishSettings::GetSteamCmdDownloadUrl() const
{
#if PLATFORM_MAC
	return SteamCmdDownloadUrlMac;
#elif PLATFORM_LINUX
	return SteamCmdDownloadUrlLinux;
#elif PLATFORM_WINDOWS
	return SteamCmdDownloadUrlWindows;
#else
	return FString();
#endif
}

FName USteamPublishSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName USteamPublishSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName USteamPublishSettings::GetSectionName() const
{
	return TEXT("SandwichSteamPublish");
}

const USteamPublishUserSettings* USteamPublishUserSettings::Get()
{
	return GetDefault<USteamPublishUserSettings>();
}

FName USteamPublishUserSettings::GetContainerName() const
{
	return TEXT("Project");
}

FName USteamPublishUserSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName USteamPublishUserSettings::GetSectionName() const
{
	return TEXT("SandwichSteamPublishUser");
}
