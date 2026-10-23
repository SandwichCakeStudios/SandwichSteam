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

FSteamResult USteamSessionsLibrary::SendSteamSessionInvite(const UObject* WorldContextObject, FSteamId Friend, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SendInvite(Friend, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::ShowSteamInviteOverlay(const UObject* WorldContextObject, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->ShowInviteOverlay(Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::TravelToSteamSession(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->TravelToSession() : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SteamServerTravel(const UObject* WorldContextObject, const FString& MapName, bool bListen, bool bAbsolute)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->ServerTravel(MapName, bListen, bAbsolute) : NotAvailable();
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

bool USteamSessionsLibrary::GetCurrentSteamLobbySettings(const UObject* WorldContextObject, FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->GetCurrentLobbySettings(OutSettings, OutProfileTag).IsSuccess();
}

FSteamResult USteamSessionsLibrary::UpdateSteamLobby(const UObject* WorldContextObject, const FSteamSessionSettings& Settings, FSteamSessionSettings& OutAppliedSettings)
{
	OutAppliedSettings = FSteamSessionSettings();
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	if (!Sessions)
	{
		return NotAvailable();
	}

	// UpdateLobby is synchronous: the delegate runs before it returns.
	return Sessions->UpdateLobby(Settings, FSteamSessionSettingsOpDelegate::CreateLambda([&OutAppliedSettings](const FSteamResult&, const FSteamSessionSettings& Applied)
	{
		OutAppliedSettings = Applied;
	}));
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

bool USteamSessionsLibrary::IsSteamLobbyOwner(const UObject* WorldContextObject, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->IsLobbyOwner(Target);
}

FSteamId USteamSessionsLibrary::GetSteamLobbyOwner(const UObject* WorldContextObject, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetLobbyOwnerId(Target) : FSteamId();
}

TArray<FSteamLobbyMember> USteamSessionsLibrary::GetSteamLobbyMembers(const UObject* WorldContextObject, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetLobbyMembers(Target) : TArray<FSteamLobbyMember>();
}

bool USteamSessionsLibrary::AreAllSteamLobbyMembersReady(const UObject* WorldContextObject, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->AreAllMembersReady(Target);
}

FString USteamSessionsLibrary::GetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, bool& bFound, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	FString Value;
	bFound = Sessions && Sessions->GetLobbyData(Key, Value, Target);
	return Value;
}

TMap<FString, FString> USteamSessionsLibrary::GetAllSteamLobbyData(const UObject* WorldContextObject, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetAllLobbyData(Target) : TMap<FString, FString>();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyData(const UObject* WorldContextObject, const FString& Key, const FString& Value, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetLobbyData(Key, Value, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::RemoveSteamLobbyData(const UObject* WorldContextObject, const FString& Key, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->RemoveLobbyData(Key, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyJoinable(const UObject* WorldContextObject, bool bJoinable, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetLobbyJoinable(bJoinable, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyMemberData(const UObject* WorldContextObject, const FString& Key, const FString& Value, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetMemberData(Key, Value, Target) : NotAvailable();
}

FString USteamSessionsLibrary::GetSteamLobbyMemberData(const UObject* WorldContextObject, FSteamId Member, const FString& Key, ESteamLobbyTarget Target)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetMemberData(Member, Key, Target) : FString();
}

FSteamResult USteamSessionsLibrary::SetSteamLobbyReady(const UObject* WorldContextObject, bool bReady, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SetReady(bReady, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::SendSteamLobbyChatMessage(const UObject* WorldContextObject, const FString& Text, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->SendChatMessage(Text, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::KickSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member, const FString& Reason, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->KickMember(Member, Reason, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::ForgiveSteamLobbyMember(const UObject* WorldContextObject, FSteamId Member, ESteamLobbyTarget Target)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->ForgiveMember(Member, Target) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::LeaveSteamLobby(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->LeaveLobby() : NotAvailable();
}

// ---- Party lobby ----

bool USteamSessionsLibrary::IsInSteamParty(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->IsInParty();
}

FSteamId USteamSessionsLibrary::GetSteamPartyLobbyId(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetPartyLobbyId() : FSteamId();
}

FSteamId USteamSessionsLibrary::GetJoinableSteamLobbyId(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->GetJoinableLobbyId() : FSteamId();
}

bool USteamSessionsLibrary::IsSteamPartyInMatch(const UObject* WorldContextObject)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions && Sessions->IsPartyInMatch();
}

bool USteamSessionsLibrary::GetSteamPartyMatch(const UObject* WorldContextObject, FSteamPartyMatch& OutMatch)
{
	const USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	if (!Sessions)
	{
		OutMatch = FSteamPartyMatch();
		return false;
	}
	return Sessions->GetPartyMatch(OutMatch);
}

FSteamResult USteamSessionsLibrary::BeginSteamPartyMatch(const UObject* WorldContextObject, const FSteamPartyMatch& Match)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->BeginPartyMatch(Match) : NotAvailable();
}

FSteamResult USteamSessionsLibrary::EndSteamPartyMatch(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->EndPartyMatch() : NotAvailable();
}

FSteamResult USteamSessionsLibrary::FollowSteamPartyMatch(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->FollowPartyMatch() : NotAvailable();
}

FSteamResult USteamSessionsLibrary::LeaveSteamParty(const UObject* WorldContextObject)
{
	USteamSessionsSubsystem* Sessions = USteamSessionsSubsystem::Get(WorldContextObject);
	return Sessions ? Sessions->LeaveLobby() : NotAvailable();
}
