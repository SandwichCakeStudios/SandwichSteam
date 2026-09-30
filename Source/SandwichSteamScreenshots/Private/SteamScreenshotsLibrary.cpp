// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamScreenshotsLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "SteamScreenshotsSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "ScreenshotsLibNotAvailable", "The Screenshots feature is not available in this game instance."));
	}
}

FSteamResult USteamScreenshotsLibrary::TriggerSteamScreenshot(const UObject* WorldContextObject)
{
	USteamScreenshotsSubsystem* Screenshots = USteamScreenshotsSubsystem::Get(WorldContextObject);
	return Screenshots ? Screenshots->TriggerScreenshot() : NotAvailable();
}

FSteamResult USteamScreenshotsLibrary::CaptureViewportToSteam(const UObject* WorldContextObject)
{
	USteamScreenshotsSubsystem* Screenshots = USteamScreenshotsSubsystem::Get(WorldContextObject);
	return Screenshots ? Screenshots->CaptureViewportToSteam() : NotAvailable();
}

void USteamScreenshotsLibrary::SetSteamScreenshotLocation(const UObject* WorldContextObject, const FString& Location)
{
	if (USteamScreenshotsSubsystem* Screenshots = USteamScreenshotsSubsystem::Get(WorldContextObject))
	{
		Screenshots->SetLocation(Location);
	}
}

FSteamResult USteamScreenshotsLibrary::TagSteamScreenshotUser(const UObject* WorldContextObject, FSteamId User)
{
	USteamScreenshotsSubsystem* Screenshots = USteamScreenshotsSubsystem::Get(WorldContextObject);
	return Screenshots ? Screenshots->TagUser(User) : NotAvailable();
}

bool USteamScreenshotsLibrary::IsSteamScreenshotHookActive(const UObject* WorldContextObject)
{
	const USteamScreenshotsSubsystem* Screenshots = USteamScreenshotsSubsystem::Get(WorldContextObject);
	return Screenshots && Screenshots->IsHooked();
}
