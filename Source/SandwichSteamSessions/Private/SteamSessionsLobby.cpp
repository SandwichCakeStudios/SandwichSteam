// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

// Lobby extras of USteamSessionsSubsystem (Phase 9b): lobby data, member data, ready state, chat, kick, owner migration and the
// party lobby (Phase 17). The lobby of a net session belongs to the Online Subsystem; everything here works on it through raw
// ISteamMatchmaking. The party lobby belongs to the plugin and may exist next to a session.

#include "SteamSessionsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "SteamLobbyBackend.h"
#include "SteamSessionRules.h"

namespace
{
	FSteamResult MakeLobbyBusyResult()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "LobbyBusy", "Another session or lobby request is still running. Wait for it to finish."));
	}

	FSteamResult MakeArgumentError(const FText& Message)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, Message);
	}

	const TCHAR* ToText(ESteamLobbyTarget Source)
	{
		return Source == ESteamLobbyTarget::Party ? TEXT("party") : TEXT("session");
	}
}

FSteamId USteamSessionsSubsystem::GetActiveLobbyId() const
{
	return GetLobbyIdFor(ESteamLobbyTarget::Active);
}

FSteamId USteamSessionsSubsystem::GetLobbyIdFor(ESteamLobbyTarget Target) const
{
	return SandwichSteam::Sessions::ResolveLobbyTarget(Target, PartyLobbyId, GetSessionLobbyId());
}

bool USteamSessionsSubsystem::IsInLobbyOnlyMode() const
{
	return PartyLobbyId.IsValid() && !IsInSession();
}

FSteamId USteamSessionsSubsystem::GetJoinableLobbyId() const
{
	return GetActiveLobbyId();
}

bool USteamSessionsSubsystem::IsKnownLobby(FSteamId Lobby) const
{
	return Lobby.IsValid() && (Lobby == PartyLobbyId || Lobby == GetSessionLobbyId());
}

ESteamLobbyTarget USteamSessionsSubsystem::SourceOf(FSteamId Lobby) const
{
	return (Lobby.IsValid() && Lobby == PartyLobbyId) ? ESteamLobbyTarget::Party : ESteamLobbyTarget::Session;
}

FSteamResult USteamSessionsSubsystem::RequireLobby(ESteamLobbyTarget Target, FSteamId& OutLobby) const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = RequireClient();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!LobbyBackend.IsValid() || !LobbyBackend->IsUsable())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyUnavailable", "The Steam matchmaking interface is not available."));
	}

	OutLobby = GetLobbyIdFor(Target);
	if (!OutLobby.IsValid())
	{
		switch (Target)
		{
		case ESteamLobbyTarget::Party:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound, NSLOCTEXT("SandwichSteam", "LobbyNoParty", "Not in a party lobby. Create or join a lobby first."));
		case ESteamLobbyTarget::Session:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound, NSLOCTEXT("SandwichSteam", "LobbyNoSessionLobby", "The current session has no lobby (no session, or a session without presence)."));
		default:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound, NSLOCTEXT("SandwichSteam", "LobbyNone", "Not in a lobby. Create or join a session or a lobby first."));
		}
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::RequireLobbyOwner(ESteamLobbyTarget Target, FSteamId& OutLobby) const
{
	const FSteamResult Result = RequireLobby(Target, OutLobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (LobbyBackend->GetOwner(OutLobby) != LobbyBackend->GetLocalId())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotOwner, NSLOCTEXT("SandwichSteam", "LobbyNotOwner", "Only the lobby owner can do this."));
	}

	return FSteamResult::Success();
}

FSteamLobbyMember USteamSessionsSubsystem::MakeLobbyMember(FSteamId Lobby, FSteamId Member, FSteamId Owner) const
{
	FSteamLobbyMember Result;
	Result.Id = Member;
	Result.bIsOwner = Member == Owner;
	if (LobbyBackend.IsValid())
	{
		Result.Name = LobbyBackend->GetMemberName(Member);
		Result.bIsLocal = Member == LobbyBackend->GetLocalId();
		Result.bReady = SandwichSteam::Sessions::ParseFlag(LobbyBackend->GetMemberData(Lobby, Member, SandwichSteam::Sessions::ReadyKey()));
	}
	return Result;
}

// ---- Party lobby: create, join, leave ----

FSteamResult USteamSessionsSubsystem::CreateLobby(const FGameplayTag& ProfileTag, FSteamLobbyOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Definition)
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so lobbies cannot be created by profile tag."));
	}

	const FSteamSessionProfileDef* Profile = Definition->FindSessionProfile(ProfileTag);
	if (!Profile)
	{
		return MakeArgumentError(FText::Format(NSLOCTEXT("SandwichSteam", "LobbyUnknownProfile", "The tag '{0}' is not a session profile of the Steam App Definition."), FText::FromName(ProfileTag.GetTagName())));
	}

	FSteamSessionSettings Defaults;
	SandwichSteam::Sessions::MakeDefaultSettings(*Profile, Defaults);
	return StartCreateLobby(ProfileTag, Profile, Defaults, FSteamLobbySettingsOpDelegate::CreateWeakLambda(this, [OnComplete](const FSteamResult& Done, FSteamId Lobby, const FSteamSessionSettings&)
	{
		OnComplete.ExecuteIfBound(Done, Lobby);
	}));
}

FSteamResult USteamSessionsSubsystem::CreateLobby(const FSteamSessionSettings& Settings, FSteamLobbySettingsOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	return StartCreateLobby(FGameplayTag(), nullptr, Settings, MoveTemp(OnComplete));
}

FSteamResult USteamSessionsSubsystem::CreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamLobbySettingsOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Definition)
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so lobbies cannot be created by profile tag."));
	}

	const FSteamSessionProfileDef* Profile = Definition->FindSessionProfile(ProfileTag);
	if (!Profile)
	{
		return MakeArgumentError(FText::Format(NSLOCTEXT("SandwichSteam", "LobbyUnknownProfile", "The tag '{0}' is not a session profile of the Steam App Definition."), FText::FromName(ProfileTag.GetTagName())));
	}

	return StartCreateLobby(ProfileTag, Profile, Settings, MoveTemp(OnComplete));
}

FSteamResult USteamSessionsSubsystem::StartCreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Requested, FSteamLobbySettingsOpDelegate OnComplete)
{
	FSteamResult Result = RequireClient();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	FSteamSessionSettings Applied;
	FText Error;
	if (!SandwichSteam::Sessions::ResolveSessionSettings(Profile, Requested, Applied, Error))
	{
		return MakeArgumentError(Error);
	}

	if (!LobbyBackend.IsValid() || !LobbyBackend->IsUsable())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyCreateUnavailable", "The Steam matchmaking interface is not available."));
	}

	if (Busy != EBusyOp::None)
	{
		return MakeLobbyBusyResult();
	}

	if (PartyLobbyId.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyAlreadyIn", "Already in a party lobby. Leave it first."));
	}

	Busy = EBusyOp::CreatingLobby;
	PendingLobbyCreate = OnComplete;
	PartyProfile = ProfileTag;
	PartySettings = Applied;
	const TWeakObjectPtr<USteamSessionsSubsystem> WeakSelf(this);
	const bool bStarted = LobbyBackend->CreateLobby(ProfileTag, Applied, [WeakSelf](const FSteamResult& Done, FSteamId Lobby)
	{
		if (USteamSessionsSubsystem* Sessions = WeakSelf.Get())
		{
			Sessions->HandleLobbyEnterComplete(Done, Lobby, /*bWasCreate*/ true);
		}
	});

	if (!bStarted)
	{
		Busy = EBusyOp::None;
		PendingLobbyCreate.Unbind();
		PartyProfile = FGameplayTag();
		PartySettings = FSteamSessionSettings();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyCreateRefused", "Steam refused to start creating the lobby."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::JoinLobby(FSteamId LobbyId, FSteamLobbyOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = RequireClient();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!LobbyId.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyJoinInvalid", "Joining a lobby needs a valid lobby Steam ID."));
	}

	if (!LobbyBackend.IsValid() || !LobbyBackend->IsUsable())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyJoinUnavailable", "The Steam matchmaking interface is not available."));
	}

	if (Busy != EBusyOp::None)
	{
		return MakeLobbyBusyResult();
	}

	if (PartyLobbyId.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyJoinAlreadyIn", "Already in a party lobby. Leave it first."));
	}

	if (LobbyId == GetSessionLobbyId())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyJoinSessionLobby", "This is the lobby of the current session. A session lobby cannot also be the party."));
	}

	Busy = EBusyOp::JoiningLobby;
	PendingLobbyJoin = OnComplete;
	const TWeakObjectPtr<USteamSessionsSubsystem> WeakSelf(this);
	const bool bStarted = LobbyBackend->JoinLobby(LobbyId, [WeakSelf](const FSteamResult& Done, FSteamId Lobby)
	{
		if (USteamSessionsSubsystem* Sessions = WeakSelf.Get())
		{
			Sessions->HandleLobbyEnterComplete(Done, Lobby, /*bWasCreate*/ false);
		}
	});

	if (!bStarted)
	{
		Busy = EBusyOp::None;
		PendingLobbyJoin.Unbind();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyJoinRefused", "Steam refused to start joining the lobby."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::FindLobbies(const FSteamSessionSearchOptions& Options, FSteamLobbyFindDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = RequireClient();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!LobbyBackend.IsValid() || !LobbyBackend->IsUsable())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyFindUnavailable", "The Steam matchmaking interface is not available."));
	}

	if (bFindingLobbies)
	{
		return MakeLobbyBusyResult();
	}

	bFindingLobbies = true;
	PendingLobbyFind = OnComplete;
	const TWeakObjectPtr<USteamSessionsSubsystem> WeakSelf(this);
	const FString NameFilter = Options.NameContains;
	const bool bStarted = LobbyBackend->RequestLobbyList(Options, [WeakSelf, NameFilter](const FSteamResult& Done, TArray<FSteamLobbyInfo>&& Lobbies)
	{
		if (!NameFilter.IsEmpty())
		{
			Lobbies.RemoveAll([&NameFilter](const FSteamLobbyInfo& Lobby) { return !Lobby.DisplayName.Contains(NameFilter); });
		}

		if (USteamSessionsSubsystem* Sessions = WeakSelf.Get())
		{
			Sessions->HandleLobbyListComplete(Done, MoveTemp(Lobbies));
		}
	});

	if (!bStarted)
	{
		bFindingLobbies = false;
		PendingLobbyFind.Unbind();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyFindRefused", "Steam refused to start the lobby search."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::LeaveLobby()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!PartyLobbyId.IsValid())
	{
		if (IsInSession())
		{
			return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyLeaveSession", "There is no party lobby. The session's lobby is left with Destroy Session."));
		}
		return FSteamResult::Success();
	}

	if (LobbyBackend.IsValid())
	{
		LobbyBackend->LeaveLobby(PartyLobbyId);
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: left party lobby %s"), *PartyLobbyId.ToString());
	ClearParty();
	return FSteamResult::Success();
}

void USteamSessionsSubsystem::ClearParty()
{
	const bool bWasInMatch = bPartyMatchActive;
	PartyLobbyId = FSteamId();
	PartyProfile = FGameplayTag();
	PartySettings = FSteamSessionSettings();
	bPartyJoinable = true;
	bPartyMatchActive = false;
	bPartyMatchFromCallback = false;
	PartyMatchCode.Reset();
	PartyMatch = FSteamPartyMatch();
	RefreshLobbyOwners();

	if (bWasInMatch)
	{
		OnPartyMatchEnded.Broadcast();
	}
}

void USteamSessionsSubsystem::HandleLobbyEnterComplete(const FSteamResult& Result, FSteamId Lobby, bool bWasCreate)
{
	if (Busy == EBusyOp::CreatingLobby || Busy == EBusyOp::JoiningLobby)
	{
		Busy = EBusyOp::None;
	}

	FSteamResult Outcome = Result;
	if (Result.IsSuccess())
	{
		PartyLobbyId = Lobby;
		bPartyJoinable = true;
		RefreshLobbyOwners();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: entered party lobby %s"), *Lobby.ToString());

		// A lobby that kicked this player before takes the player back for a moment: leave again.
		if (CheckKicked(Lobby))
		{
			Outcome = FSteamResult::Failure(SteamGameplayTags::Error_Lobby_Denied, NSLOCTEXT("SandwichSteam", "LobbyKickedBefore", "The lobby owner kicked this player from the lobby."));
		}
	}
	else
	{
		PartyProfile = FGameplayTag();
		PartySettings = FSteamSessionSettings();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby request failed: %s"), *Result.Message.ToString());
	}

	if (bWasCreate)
	{
		const FSteamLobbySettingsOpDelegate Done = PendingLobbyCreate;
		PendingLobbyCreate.Unbind();
		Done.ExecuteIfBound(Outcome, Outcome.IsSuccess() ? Lobby : FSteamId(), PartySettings);
	}
	else
	{
		const FSteamLobbyOpDelegate Done = PendingLobbyJoin;
		PendingLobbyJoin.Unbind();
		Done.ExecuteIfBound(Outcome, Outcome.IsSuccess() ? Lobby : FSteamId());
	}

	// A party that is already in a match: tell the game (and follow, by the setting) after it heard about the join.
	if (Outcome.IsSuccess())
	{
		RefreshPartyMatch();
	}
}

void USteamSessionsSubsystem::HandleLobbyListComplete(const FSteamResult& Result, TArray<FSteamLobbyInfo>&& Lobbies)
{
	bFindingLobbies = false;
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby search %s, %d lobby(ies)"), Result.IsSuccess() ? TEXT("finished") : TEXT("failed"), Lobbies.Num());

	const FSteamLobbyFindDelegate Done = PendingLobbyFind;
	PendingLobbyFind.Unbind();
	Done.ExecuteIfBound(Result, Lobbies);
}

// ---- Party lobby settings ----

FSteamResult USteamSessionsSubsystem::UpdateLobby(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete)
{
	FSteamId Lobby;
	FSteamResult Result = RequireLobbyOwner(ESteamLobbyTarget::Party, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const FSteamSessionProfileDef* Profile = (Definition && PartyProfile.IsValid()) ? Definition->FindSessionProfile(PartyProfile) : nullptr;
	if (PartyProfile.IsValid() && !Profile)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyUpdateProfileGone", "The session profile the lobby was created from is no longer in the Steam App Definition."));
	}

	FSteamSessionSettings Applied;
	FText Error;
	if (!SandwichSteam::Sessions::ValidateSessionUpdate(Profile, PartySettings, Settings, GetLobbyMembers(ESteamLobbyTarget::Party).Num(), Applied, Error))
	{
		return MakeArgumentError(Error);
	}

	// Raw ISteamMatchmaking calls, all synchronous (no call result to wait for). A running party match keeps its own lobby type:
	// the new visibility goes into the saved state and applies when the match ends.
	PartySettings = Applied;
	bool bOk = LobbyBackend->SetLobbyMemberLimit(Lobby, Applied.MaxPlayers);
	SandwichSteam::Sessions::FPartyLobbyState Saved;
	FString SavedText;
	if (IsPartyInMatch() && LobbyBackend->GetLobbyData(Lobby, SandwichSteam::Sessions::VisibilityKey(), SavedText) && SandwichSteam::Sessions::DecodeSavedLobbyState(SavedText, Saved))
	{
		Saved.Type = SandwichSteam::Sessions::ToPartyLobbyType(Applied.Visibility);
		bOk &= LobbyBackend->SetLobbyData(Lobby, SandwichSteam::Sessions::VisibilityKey(), SandwichSteam::Sessions::EncodeSavedLobbyState(Saved));
	}
	else
	{
		bOk &= LobbyBackend->SetLobbyType(Lobby, Applied.Visibility);
	}
	bOk &= LobbyBackend->SetLobbyData(Lobby, SandwichSteam::Sessions::NameKey(), Applied.DisplayName);
	for (const TPair<FName, FString>& Pair : Applied.Settings)
	{
		bOk &= LobbyBackend->SetLobbyData(Lobby, Pair.Key.ToString(), Pair.Value);
	}

	Result = bOk
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsUpdateLobbyFailed", "Steam refused one of the lobby update calls."));
	OnComplete.ExecuteIfBound(Result, Applied);
	return Result;
}

FSteamResult USteamSessionsSubsystem::GetCurrentLobbySettings(FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag) const
{
	OutSettings = FSteamSessionSettings();
	OutProfileTag = FGameplayTag();

	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!PartyLobbyId.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyNoCurrent", "There is no party lobby."));
	}

	if (IsLobbyOwner(ESteamLobbyTarget::Party) && PartySettings.MaxPlayers > 0)
	{
		OutSettings = PartySettings;
		OutProfileTag = PartyProfile;
		return FSteamResult::Success();
	}

	// A member of somebody else's party (or an owner by migration): reconstruct what Steam can tell (Visibility is not readable back, known limit).
	OutSettings.MaxPlayers = GetLobbyMemberLimit(ESteamLobbyTarget::Party);
	OutSettings.bUsesPresence = true;
	for (const TPair<FString, FString>& Pair : GetAllLobbyData(ESteamLobbyTarget::Party))
	{
		if (Pair.Key.Equals(SandwichSteam::Sessions::NameKey(), ESearchCase::IgnoreCase))
		{
			OutSettings.DisplayName = Pair.Value;
		}
		else if (Pair.Key.Equals(SandwichSteam::Sessions::ProfileKey(), ESearchCase::IgnoreCase))
		{
			OutProfileTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Pair.Value), /*ErrorIfNotFound*/ false);
		}
		else if (!SandwichSteam::Sessions::IsReservedLobbyKey(Pair.Key))
		{
			OutSettings.Settings.Add(FName(*Pair.Key), Pair.Value);
		}
	}
	return FSteamResult::Success();
}

// ---- Reading ----

FSteamId USteamSessionsSubsystem::GetLobbyOwnerId(ESteamLobbyTarget Target) const
{
	const FSteamId Lobby = GetLobbyIdFor(Target);
	return (LobbyBackend.IsValid() && Lobby.IsValid()) ? LobbyBackend->GetOwner(Lobby) : FSteamId();
}

bool USteamSessionsSubsystem::IsLobbyOwner(ESteamLobbyTarget Target) const
{
	const FSteamId Owner = GetLobbyOwnerId(Target);
	return Owner.IsValid() && LobbyBackend.IsValid() && Owner == LobbyBackend->GetLocalId();
}

int32 USteamSessionsSubsystem::GetLobbyMemberLimit(ESteamLobbyTarget Target) const
{
	const FSteamId Lobby = GetLobbyIdFor(Target);
	return (LobbyBackend.IsValid() && Lobby.IsValid()) ? LobbyBackend->GetMemberLimit(Lobby) : 0;
}

TArray<FSteamLobbyMember> USteamSessionsSubsystem::GetLobbyMembers(ESteamLobbyTarget Target) const
{
	TArray<FSteamLobbyMember> Members;
	const FSteamId Lobby = GetLobbyIdFor(Target);
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
	{
		return Members;
	}

	TArray<FSteamId> Ids;
	LobbyBackend->GetMemberIds(Lobby, Ids);
	const FSteamId Owner = LobbyBackend->GetOwner(Lobby);
	Members.Reserve(Ids.Num());
	for (const FSteamId& Id : Ids)
	{
		Members.Add(MakeLobbyMember(Lobby, Id, Owner));
	}
	return Members;
}

bool USteamSessionsSubsystem::GetLobbyData(const FString& Key, FString& OutValue, ESteamLobbyTarget Target) const
{
	const FSteamId Lobby = GetLobbyIdFor(Target);
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
	{
		OutValue.Reset();
		return false;
	}
	return LobbyBackend->GetLobbyData(Lobby, Key, OutValue);
}

TMap<FString, FString> USteamSessionsSubsystem::GetAllLobbyData(ESteamLobbyTarget Target) const
{
	TMap<FString, FString> Data;
	const FSteamId Lobby = GetLobbyIdFor(Target);
	if (LobbyBackend.IsValid() && Lobby.IsValid())
	{
		LobbyBackend->GetAllLobbyData(Lobby, Data);
	}
	return Data;
}

FString USteamSessionsSubsystem::GetMemberData(FSteamId Member, const FString& Key, ESteamLobbyTarget Target) const
{
	const FSteamId Lobby = GetLobbyIdFor(Target);
	return (LobbyBackend.IsValid() && Lobby.IsValid()) ? LobbyBackend->GetMemberData(Lobby, Member, Key) : FString();
}

bool USteamSessionsSubsystem::AreAllMembersReady(ESteamLobbyTarget Target) const
{
	const TArray<FSteamLobbyMember> Members = GetLobbyMembers(Target);
	if (Members.IsEmpty())
	{
		return false;
	}

	for (const FSteamLobbyMember& Member : Members)
	{
		if (!Member.bReady)
		{
			return false;
		}
	}
	return true;
}

// ---- Writing ----

FSteamResult USteamSessionsSubsystem::SetLobbyData(const FString& Key, const FString& Value, ESteamLobbyTarget Target)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateLobbyKey(Key, Error) || !SandwichSteam::Sessions::ValidateLobbyValue(Value, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->SetLobbyData(Lobby, Key, Value)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbySetDataFailed", "Steam refused to set the lobby data."));
}

FSteamResult USteamSessionsSubsystem::RemoveLobbyData(const FString& Key, ESteamLobbyTarget Target)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateLobbyKey(Key, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->DeleteLobbyData(Lobby, Key)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyRemoveDataFailed", "Steam refused to remove the lobby data."));
}

FSteamResult USteamSessionsSubsystem::SetLobbyJoinable(bool bJoinable, ESteamLobbyTarget Target)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!LobbyBackend->SetJoinable(Lobby, bJoinable))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyJoinableFailed", "Steam refused to change whether the lobby can be joined."));
	}

	if (Lobby == PartyLobbyId)
	{
		bPartyJoinable = bJoinable;
	}
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::SetMemberData(const FString& Key, const FString& Value, ESteamLobbyTarget Target)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateLobbyKey(Key, Error) || !SandwichSteam::Sessions::ValidateLobbyValue(Value, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobby(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	LobbyBackend->SetMemberData(Lobby, Key, Value);
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::SetReady(bool bReady, ESteamLobbyTarget Target)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobby(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	LobbyBackend->SetMemberData(Lobby, SandwichSteam::Sessions::ReadyKey(), bReady ? TEXT("1") : TEXT("0"));
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::SendChatMessage(const FString& Text, ESteamLobbyTarget Target)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateChatText(Text, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobby(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->SendChat(Lobby, Text)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyChatFailed", "Steam could not send the chat message."));
}

// ---- Kick ----

FSteamResult USteamSessionsSubsystem::KickMember(FSteamId Member, const FString& Reason, ESteamLobbyTarget Target)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!Member.IsValid() || Member == LobbyBackend->GetLocalId())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyKickArgs", "Kick needs the Steam ID of another player."));
	}

	const FString Value = Reason.IsEmpty() ? FString(TEXT("1")) : Reason.Left(200);
	if (!LobbyBackend->SetLobbyData(Lobby, SandwichSteam::Sessions::KickKey(Member), Value))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyKickFailed", "Steam refused to write the kick marker."));
	}

	const ESteamLobbyTarget Source = SourceOf(Lobby);
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: kicked %s from the %s lobby %s"), *Member.ToString(), ToText(Source), *Lobby.ToString());
	if (Source == ESteamLobbyTarget::Session && IsInSession())
	{
		KickFromGameSession(Member, FText::FromString(Reason));
	}
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::ForgiveMember(FSteamId Member, ESteamLobbyTarget Target)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Target, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!Member.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyForgiveArgs", "Forgive needs the Steam ID of a player."));
	}

	LobbyBackend->DeleteLobbyData(Lobby, SandwichSteam::Sessions::KickKey(Member));
	return FSteamResult::Success();
}

bool USteamSessionsSubsystem::KickFromGameSession(FSteamId Member, const FText& Reason)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!GameMode || !GameMode->GameSession)
	{
		return false; // Not the host of a game: only the lobby marker applies.
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		const APlayerState* State = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
		if (!State)
		{
			continue;
		}

		FSteamId Id;
		const FUniqueNetIdRepl& NetId = State->GetUniqueId();
		if (NetId.IsValid() && FSteamId::FromUniqueNetId(*NetId.GetUniqueNetId(), Id) && Id == Member)
		{
			return GameMode->GameSession->KickPlayer(Controller, Reason);
		}
	}

	return false;
}

bool USteamSessionsSubsystem::CheckKicked(FSteamId Lobby)
{
	if (!LobbyBackend.IsValid() || !IsKnownLobby(Lobby))
	{
		return false;
	}

	const FSteamId Local = LobbyBackend->GetLocalId();
	if (!Local.IsValid() || LobbyBackend->GetOwner(Lobby) == Local)
	{
		return false; // The owner cannot be kicked.
	}

	FString Marker;
	if (!LobbyBackend->GetLobbyData(Lobby, SandwichSteam::Sessions::KickKey(Local), Marker))
	{
		return false;
	}

	const ESteamLobbyTarget Source = SourceOf(Lobby);
	const FString Reason = Marker == TEXT("1") ? FString() : Marker;
	UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: the lobby owner kicked this player from the %s lobby %s%s%s"), ToText(Source), *Lobby.ToString(), Reason.IsEmpty() ? TEXT("") : TEXT(": "), *Reason);

	if (Source == ESteamLobbyTarget::Party)
	{
		// The party only: a running session is not touched.
		LobbyBackend->LeaveLobby(Lobby);
		ClearParty();
		OnLobbyKicked.Broadcast(Reason, Source);
		return true;
	}

	OnLobbyKicked.Broadcast(Reason, Source);

	// A session lobby: ending the session leaves the lobby, and OnSessionLost tells the game to go back. The party stays.
	HandleSessionLost(FGameplayTag(SteamGameplayTags::Error_Session_Kicked), Reason.IsEmpty() ? FString(TEXT("Kicked by the lobby owner")) : Reason);
	return true;
}

// ---- Steam callbacks (game thread, through the dispatcher) ----

void USteamSessionsSubsystem::RefreshLobbyOwners()
{
	RefreshLobbyOwner(TrackedParty, PartyLobbyId, ESteamLobbyTarget::Party);
	RefreshLobbyOwner(TrackedSession, GetSessionLobbyId(), ESteamLobbyTarget::Session);
}

void USteamSessionsSubsystem::RefreshLobbyOwner(FTrackedLobby& Tracked, FSteamId Lobby, ESteamLobbyTarget Source)
{
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
	{
		Tracked = FTrackedLobby();
		return;
	}

	const FSteamId Owner = LobbyBackend->GetOwner(Lobby);
	if (Lobby != Tracked.Lobby)
	{
		// A new lobby: remember its owner, no event.
		Tracked.Lobby = Lobby;
		Tracked.Owner = Owner;
		return;
	}

	if (Owner.IsValid() && Owner != Tracked.Owner)
	{
		const FSteamId Previous = Tracked.Owner;
		Tracked.Owner = Owner;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: %s lobby owner changed from %s to %s"), ToText(Source), *Previous.ToString(), *Owner.ToString());
		OnLobbyOwnerChanged.Broadcast(Owner, Previous, Source);
	}
}

void USteamSessionsSubsystem::HandleLobbyDataUpdate(FSteamId Lobby, FSteamId Member, bool bSuccess)
{
	// The answer to a join request's RequestLobbyData (a lobby the player is not in yet).
	if (PartyLookup.IsSet() && PartyLookup->LobbyId == Lobby && Member == Lobby)
	{
		ResolvePartyLookup(bSuccess);
	}

	if (!bSuccess || !LobbyBackend.IsValid() || !IsKnownLobby(Lobby))
	{
		return;
	}

	const ESteamLobbyTarget Source = SourceOf(Lobby);
	if (Member == Lobby)
	{
		// Data of the lobby itself.
		RefreshLobbyOwners();
		if (CheckKicked(Lobby))
		{
			return;
		}

		OnLobbyDataChanged.Broadcast(Source);
		if (Source == ESteamLobbyTarget::Party)
		{
			RefreshPartyMatch();
		}
	}
	else
	{
		OnLobbyMemberDataChanged.Broadcast(MakeLobbyMember(Lobby, Member, LobbyBackend->GetOwner(Lobby)), Source);
	}
}

void USteamSessionsSubsystem::HandleLobbyChatUpdate(FSteamId Lobby, FSteamId Changed, uint32 Flags)
{
	if (!LobbyBackend.IsValid() || !IsKnownLobby(Lobby))
	{
		return;
	}

	const ESteamLobbyTarget Source = SourceOf(Lobby);
	const ESteamLobbyMemberChange Change = SandwichSteam::Sessions::MemberChangeFromFlags(Flags);
	FSteamLobbyMember Member = MakeLobbyMember(Lobby, Changed, LobbyBackend->GetOwner(Lobby));
	if (Change != ESteamLobbyMemberChange::Entered)
	{
		Member.bReady = false; // Gone: its data is not readable any more.
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: %s lobby member %s %s (change %d)"), ToText(Source), *Changed.ToString(), *Member.Name, static_cast<int32>(Change));
	OnLobbyMemberChanged.Broadcast(Member, Change, Source);

	// Steam gives the lobby to another member when the owner leaves; that shows up here first.
	RefreshLobbyOwners();
}

void USteamSessionsSubsystem::HandleLobbyChatMessage(FSteamId Lobby, FSteamId Sender, const FString& Text)
{
	if (!LobbyBackend.IsValid() || !IsKnownLobby(Lobby))
	{
		return;
	}

	const ESteamLobbyTarget Source = SourceOf(Lobby);
	++ChatReceivedCount;
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: %s lobby chat from %s: %s"), ToText(Source), *Sender.ToString(), *Text);

	FSteamLobbyChatMessage Message;
	Message.Sender = Sender;
	Message.SenderName = LobbyBackend->GetMemberName(Sender);
	Message.Text = Text;
	Message.bIsLocal = Sender == LobbyBackend->GetLocalId();
	OnLobbyChatReceived.Broadcast(Message, Source);
}
