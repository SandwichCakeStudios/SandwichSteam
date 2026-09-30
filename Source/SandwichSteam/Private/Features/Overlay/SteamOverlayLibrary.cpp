// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Overlay/SteamOverlayLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "Features/Overlay/SteamOverlaySubsystem.h"

namespace
{
	FSteamResult MakeMissingFeatureResult()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled,
			NSLOCTEXT("SandwichSteam", "OverlayFeatureMissing", "The Steam overlay feature is disabled or not available on this platform."));
	}
}

FSteamResult USteamOverlayLibrary::OpenSteamOverlay(const UObject* WorldContextObject, ESteamOverlayDialog Dialog)
{
	USteamOverlaySubsystem* Overlay = USteamOverlaySubsystem::Get(WorldContextObject);
	return Overlay ? Overlay->OpenDialog(Dialog) : MakeMissingFeatureResult();
}

FSteamResult USteamOverlayLibrary::OpenSteamOverlayTargetDialog(const UObject* WorldContextObject, ESteamOverlayDialog Dialog, FSteamId Target)
{
	USteamOverlaySubsystem* Overlay = USteamOverlaySubsystem::Get(WorldContextObject);
	return Overlay ? Overlay->OpenTargetDialog(Dialog, Target) : MakeMissingFeatureResult();
}

FSteamResult USteamOverlayLibrary::OpenSteamOverlayStore(const UObject* WorldContextObject, int32 AppId, ESteamOverlayStoreFlag Flag)
{
	USteamOverlaySubsystem* Overlay = USteamOverlaySubsystem::Get(WorldContextObject);
	return Overlay ? Overlay->OpenStore(AppId, Flag) : MakeMissingFeatureResult();
}

FSteamResult USteamOverlayLibrary::OpenSteamOverlayWebPage(const UObject* WorldContextObject, const FString& Url, bool bModal)
{
	USteamOverlaySubsystem* Overlay = USteamOverlaySubsystem::Get(WorldContextObject);
	return Overlay ? Overlay->OpenWebPage(Url, bModal) : MakeMissingFeatureResult();
}

bool USteamOverlayLibrary::IsSteamOverlayEnabled(const UObject* WorldContextObject)
{
	const USteamOverlaySubsystem* Overlay = USteamOverlaySubsystem::Get(WorldContextObject);
	return Overlay && Overlay->IsOverlayEnabled();
}

bool USteamOverlayLibrary::IsSteamOverlayActive(const UObject* WorldContextObject)
{
	const USteamOverlaySubsystem* Overlay = USteamOverlaySubsystem::Get(WorldContextObject);
	return Overlay && Overlay->IsOverlayActive();
}
