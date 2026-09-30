// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamLiveCodingGuard.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif

FSteamLiveCodingGuard::~FSteamLiveCodingGuard()
{
	FString Ignored;
	Release(Ignored);
}

bool FSteamLiveCodingGuard::Acquire(FString& OutMessage)
{
	OutMessage.Reset();
#if PLATFORM_WINDOWS
	if (bHeld)
	{
		return false;
	}
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
	if (!LiveCoding)
	{
		return false;
	}

	bWasEnabledByDefault = LiveCoding->IsEnabledByDefault();
	bWasEnabledForSession = LiveCoding->IsEnabledForSession();
	if (!bWasEnabledByDefault && !bWasEnabledForSession)
	{
		return false;
	}

	// Session first, then the saved setting; both are restored from what was read above.
	LiveCoding->EnableForSession(false);
	LiveCoding->EnableByDefault(false);
	bHeld = true;

	OutMessage = TEXT("Live Coding was enabled: disabled it while packaging, it is restored afterwards.");
	if (LiveCoding->HasStarted())
	{
		OutMessage += TEXT(" Warning: Live Coding had already started in this editor session and may keep running until the editor restarts; if UAT fails because of it, restart the editor and publish again.");
	}
	return true;
#else
	return false;
#endif
}

bool FSteamLiveCodingGuard::Release(FString& OutMessage)
{
	OutMessage.Reset();
#if PLATFORM_WINDOWS
	if (!bHeld)
	{
		return false;
	}
	bHeld = false;

	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(TEXT("LiveCoding"));
	if (!LiveCoding)
	{
		return false;
	}

	LiveCoding->EnableByDefault(bWasEnabledByDefault);
	if (bWasEnabledForSession && LiveCoding->CanEnableForSession())
	{
		LiveCoding->EnableForSession(true);
	}
	OutMessage = TEXT("Live Coding restored.");
	return true;
#else
	return false;
#endif
}
