// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAuthLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "GameFramework/PlayerState.h"
#include "SteamAuthSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "AuthLibNotAvailable", "The Auth feature is not available in this game instance."));
	}
}

bool USteamAuthLibrary::IsSteamAuthActive(const UObject* WorldContextObject)
{
	const USteamAuthSubsystem* Auth = USteamAuthSubsystem::Get(WorldContextObject);
	return Auth && Auth->IsFeatureActive();
}

ESteamAuthStatus USteamAuthLibrary::GetSteamAuthStatus(const UObject* WorldContextObject, FSteamId Player)
{
	const USteamAuthSubsystem* Auth = USteamAuthSubsystem::Get(WorldContextObject);
	return Auth ? Auth->GetAuthStatus(Player) : ESteamAuthStatus::NotChecked;
}

bool USteamAuthLibrary::GetSteamAuthResult(const UObject* WorldContextObject, FSteamId Player, FSteamAuthResult& Result)
{
	const USteamAuthSubsystem* Auth = USteamAuthSubsystem::Get(WorldContextObject);
	return Auth && Auth->GetLastResult(Player, Result);
}

bool USteamAuthLibrary::IsPlayerSteamAuthenticated(const APlayerState* PlayerState)
{
	const USteamAuthSubsystem* Auth = USteamAuthSubsystem::Get(PlayerState);
	return Auth && Auth->IsPlayerAuthenticated(PlayerState);
}

FSteamResult USteamAuthLibrary::KickSteamPlayer(const UObject* WorldContextObject, FSteamId Player, FText Reason)
{
	USteamAuthSubsystem* Auth = USteamAuthSubsystem::Get(WorldContextObject);
	return Auth ? Auth->KickPlayer(Player, Reason) : NotAvailable();
}
