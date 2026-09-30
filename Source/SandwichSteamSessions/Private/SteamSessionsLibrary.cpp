// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamSessionsLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "SteamSessionsSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "SessionsLibNotAvailable", "The Sessions feature is not available in this game instance."));
	}
}

bool USteamSessionsLibrary::IsInSteamSession(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->IsInSession();
}

FSteamId USteamSessionsLibrary::GetSteamSessionLobbyId(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetLobbyId() : FSteamId();
}

FSteamResult USteamSessionsLibrary::SendSteamSessionInvite(const UObject* WorldContextObject, FSteamId Friend)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SendInvite(Friend) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::ShowSteamInviteOverlay(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->ShowInviteOverlay() : NotAvailable();
}

FSteamResult USteamSessionsLibrary::TravelToSteamSession(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->TravelToSession() : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SteamServerTravel(const UObject* WorldContextObject, const FString& MapName, bool bListen)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->ServerTravel(MapName, bListen) : NotAvailable();
}

bool USteamSessionsLibrary::GetPendingSteamJoinRequest(const UObject* WorldContextObject, FSteamJoinRequest& OutRequest)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	if (!Sessions)
	{
		OutRequest = FSteamJoinRequest();
		return false;
	}
	return Sessions->GetPendingJoinRequest(OutRequest);
}

FSteamResult USteamSessionsLibrary::AcceptSteamJoinRequest(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->AcceptJoinRequest() : NotAvailable();
}

void USteamSessionsLibrary::DeclineSteamJoinRequest(const UObject* WorldContextObject)
{
	if (USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject))
	{
		Sessions->DeclineJoinRequest();
	}
}

bool USteamSessionsLibrary::MakeSteamSessionSettingsFromProfile(const UObject* WorldContextObject, FGameplayTag ProfileTag, FSteamSessionSettings& OutSettings)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->MakeSettingsFromProfile(ProfileTag, OutSettings).IsSuccess();
}

bool USteamSessionsLibrary::GetSteamSessionProfile(const UObject* WorldContextObject, FGameplayTag ProfileTag, FSteamSessionProfileDef& OutProfile)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->GetProfile(ProfileTag, OutProfile).IsSuccess();
}

bool USteamSessionsLibrary::GetCurrentSteamSessionSettings(const UObject* WorldContextObject, FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->GetCurrentSettings(OutSettings, OutProfileTag).IsSuccess();
}

// ---- Lobby extras ----

FSteamId USteamSessionsLibrary::GetActiveSteamLobbyId(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetActiveLobbyId() : FSteamId();
}

bool USteamSessionsLibrary::IsInSteamLobbyOnlyMode(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->IsInLobbyOnlyMode();
}

bool USteamSessionsLibrary::IsSteamLobbyOwner(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->IsLobbyOwner();
}

FSteamId USteamSessionsLibrary::GetSteamLobbyOwner(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetLobbyOwnerId() : FSteamId();
}

TArray<FSteamLobbyMember> USteamSessionsLibrary::GetSteamLobbyMembers(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetLobbyMembers() : TArray<FSteamLobbyMember>();
}

bool USteamSessionsLibrary::AreAllSteamLobbyMembersReady(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->AreAllMembersReady();
}

FString USteamSessionsLibrary::GetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, bool& bFound)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	FString Value;
	bFound = Sessions && Sessions->GetLobbyData(Key, Value);
	return Value;
}

TMap<FString, FString> USteamSessionsLibrary::GetAllSteamLobbyData(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetAllLobbyData() : TMap<FString, FString>();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, const FString& Value)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetLobbyData(Key, Value) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::RemoveSteamLobbyData(const UObject* WorldContextObject, const FString& Key)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->RemoveLobbyData(Key) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyJoinable(const UObject* WorldContextObject, bool bJoinable)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetLobbyJoinable(bJoinable) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyMemberData(const UObject* WorldContextObject, const FString& Key, const FString& Value)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetMemberData(Key, Value) : NotAvailable();
}

FString USteamSessionsLibrary::GetSteamLobbyMemberData(const UObject* WorldContextObject, FSteamId Member, const FString& Key)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetMemberData(Member, Key) : FString();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyReady(const UObject* WorldContextObject, bool bReady)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetReady(bReady) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SendSteamLobbyChatMessage(const UObject* WorldContextObject, const FString& Text)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SendChatMessage(Text) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::KickSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member, const FString& Reason)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->KickMember(Member, Reason) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::ForgiveSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->ForgiveMember(Member) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::LeaveSteamLobby(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->LeaveLobby() : NotAvailable();
}
