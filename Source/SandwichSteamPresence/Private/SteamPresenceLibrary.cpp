// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamPresenceLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamNameWarnings.h"
#include "SteamPresenceSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "PresenceLibNotAvailable", "The Presence feature is not available in this game instance."));
	}
}

FSteamResult USteamPresenceLibrary::SetSteamPresence(const UObject* WorldContextObject, FGameplayTag Status, const TMap<FName, FString>& Args)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->SetPresenceByTag(Status, Args) : NotAvailable();
}

FSteamResult USteamPresenceLibrary::SetSteamPresenceByToken(const UObject* WorldContextObject, const FString& Token, const TMap<FName, FString>& Args)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? SandwichSteam::WarnOnceIfInvalid(TEXT("presence token"), Token, Presence->SetPresenceByToken(Token, Args)) : NotAvailable();
}

FSteamResult USteamPresenceLibrary::SetSteamPresenceValue(const UObject* WorldContextObject, FName Key, const FString& Value)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->SetPresenceValue(Key, Value) : NotAvailable();
}

FSteamResult USteamPresenceLibrary::RemoveSteamPresenceValue(const UObject* WorldContextObject, FName Key)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->RemovePresenceValue(Key) : NotAvailable();
}

FSteamResult USteamPresenceLibrary::SetSteamPresenceGroup(const UObject* WorldContextObject, const FString& GroupId, int32 Size)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->SetGroup(GroupId, Size) : NotAvailable();
}

FSteamResult USteamPresenceLibrary::SetSteamConnectString(const UObject* WorldContextObject, const FString& ConnectString)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->SetConnectString(ConnectString) : NotAvailable();
}

FSteamResult USteamPresenceLibrary::ClearSteamPresence(const UObject* WorldContextObject)
{
	USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->ClearPresence() : NotAvailable();
}

FString USteamPresenceLibrary::GetSteamFriendPresenceValue(const UObject* WorldContextObject, FSteamId Friend, FName Key)
{
	const USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject);
	return Presence ? Presence->GetFriendPresenceValue(Friend, Key) : FString();
}

FSteamPresenceValues USteamPresenceLibrary::GetSteamFriendPresence(const UObject* WorldContextObject, FSteamId Friend)
{
	FSteamPresenceValues Result;
	if (const USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject))
	{
		Result.Values = Presence->GetFriendPresence(Friend);
	}
	return Result;
}

void USteamPresenceLibrary::RequestSteamFriendPresence(const UObject* WorldContextObject, FSteamId User)
{
	if (USteamPresenceSubsystem* Presence = USteamPresenceSubsystem::Get(WorldContextObject))
	{
		Presence->RequestFriendPresence(User);
	}
}
