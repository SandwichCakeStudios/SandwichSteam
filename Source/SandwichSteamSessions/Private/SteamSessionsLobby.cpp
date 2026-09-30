// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

// Lobby extras of USteamSessionsSubsystem (Phase 9b): lobby data, member data, ready state, chat, kick, owner migration and
// lobby only mode. The lobby of a net session belongs to the Online Subsystem; everything here works on it through raw ISteamMatchmaking.

#include "SteamSessionsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
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
}

FSteamId USteamSessionsSubsystem::GetActiveLobbyId() const
{
	return LobbyOnlyId.IsValid() ? LobbyOnlyId : GetLobbyId();
}

FSteamResult USteamSessionsSubsystem::RequireLobby(FSteamId& OutLobby) const
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

	OutLobby = GetActiveLobbyId();
	if (!OutLobby.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound, NSLOCTEXT("SandwichSteam", "LobbyNone", "Not in a lobby. Create or join a session or a lobby first."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::RequireLobbyOwner(FSteamId& OutLobby) const
{
	const FSteamResult Result = RequireLobby(OutLobby);
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

// ---- Lobby only mode ----

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

	if (IsInSession() || LobbyOnlyId.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyAlreadyIn", "Already in a session or lobby. Leave it first."));
	}

	Busy = EBusyOp::CreatingLobby;
	PendingLobbyCreate = OnComplete;
	ActiveProfile = ProfileTag;
	HostedSettings = Applied;
	const TWeakObjectPtr<USteamSessionsSubsystem> WeakThis(this);
	const bool bStarted = LobbyBackend->CreateLobby(ProfileTag, Applied, [WeakThis](const FSteamResult& Done, FSteamId Lobby)
	{
		if (USteamSessionsSubsystem* Sessions = WeakThis.Get())
		{
			Sessions->HandleLobbyEnterComplete(Done, Lobby, /*bWasCreate*/ true);
		}
	});

	if (!bStarted)
	{
		Busy = EBusyOp::None;
		PendingLobbyCreate.Unbind();
		ActiveProfile = FGameplayTag();
		HostedSettings = FSteamSessionSettings();
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

	if (IsInSession() || LobbyOnlyId.IsValid())
	{
		return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyJoinAlreadyIn", "Already in a session or lobby. Leave it first."));
	}

	Busy = EBusyOp::JoiningLobby;
	PendingLobbyJoin = OnComplete;
	const TWeakObjectPtr<USteamSessionsSubsystem> WeakThis(this);
	const bool bStarted = LobbyBackend->JoinLobby(LobbyId, [WeakThis](const FSteamResult& Done, FSteamId Lobby)
	{
		if (USteamSessionsSubsystem* Sessions = WeakThis.Get())
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
	const TWeakObjectPtr<USteamSessionsSubsystem> WeakThis(this);
	const FString NameFilter = Options.NameContains;
	const bool bStarted = LobbyBackend->RequestLobbyList(Options, [WeakThis, NameFilter](const FSteamResult& Done, TArray<FSteamLobbyInfo>&& Lobbies)
	{
		if (!NameFilter.IsEmpty())
		{
			Lobbies.RemoveAll([&NameFilter](const FSteamLobbyInfo& Lobby) { return !Lobby.DisplayName.Contains(NameFilter); });
		}

		if (USteamSessionsSubsystem* Sessions = WeakThis.Get())
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

	if (!LobbyOnlyId.IsValid())
	{
		if (IsInSession())
		{
			return MakeArgumentError(NSLOCTEXT("SandwichSteam", "LobbyLeaveSession", "The lobby belongs to a session. Destroy the session instead."));
		}
		return FSteamResult::Success();
	}

	if (LobbyBackend.IsValid())
	{
		LobbyBackend->LeaveLobby(LobbyOnlyId);
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: left lobby %s"), *LobbyOnlyId.ToString());
	LobbyOnlyId = FSteamId();
	ActiveProfile = FGameplayTag();
	HostedSettings = FSteamSessionSettings();
	RefreshLobbyOwner();
	return FSteamResult::Success();
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
		LobbyOnlyId = Lobby;
		RefreshLobbyOwner();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: entered lobby %s (lobby only mode)"), *Lobby.ToString());

		// A lobby that kicked this player before takes the player back for a moment: leave again.
		if (CheckKicked())
		{
			Outcome = FSteamResult::Failure(SteamGameplayTags::Error_Lobby_Denied, NSLOCTEXT("SandwichSteam", "LobbyKickedBefore", "The lobby owner kicked this player from the lobby."));
		}
	}
	else
	{
		ActiveProfile = FGameplayTag();
		HostedSettings = FSteamSessionSettings();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby request failed: %s"), *Result.Message.ToString());
	}

	if (bWasCreate)
	{
		const FSteamLobbySettingsOpDelegate Done = PendingLobbyCreate;
		PendingLobbyCreate.Unbind();
		Done.ExecuteIfBound(Outcome, Outcome.IsSuccess() ? Lobby : FSteamId(), HostedSettings);
	}
	else
	{
		const FSteamLobbyOpDelegate Done = PendingLobbyJoin;
		PendingLobbyJoin.Unbind();
		Done.ExecuteIfBound(Outcome, Outcome.IsSuccess() ? Lobby : FSteamId());
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

// ---- Reading ----

FSteamId USteamSessionsSubsystem::GetLobbyOwnerId() const
{
	const FSteamId Lobby = GetActiveLobbyId();
	return (LobbyBackend.IsValid() && Lobby.IsValid()) ? LobbyBackend->GetOwner(Lobby) : FSteamId();
}

bool USteamSessionsSubsystem::IsLobbyOwner() const
{
	const FSteamId Owner = GetLobbyOwnerId();
	return Owner.IsValid() && LobbyBackend.IsValid() && Owner == LobbyBackend->GetLocalId();
}

int32 USteamSessionsSubsystem::GetLobbyMemberLimit() const
{
	const FSteamId Lobby = GetActiveLobbyId();
	return (LobbyBackend.IsValid() && Lobby.IsValid()) ? LobbyBackend->GetMemberLimit(Lobby) : 0;
}

TArray<FSteamLobbyMember> USteamSessionsSubsystem::GetLobbyMembers() const
{
	TArray<FSteamLobbyMember> Members;
	const FSteamId Lobby = GetActiveLobbyId();
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

bool USteamSessionsSubsystem::GetLobbyData(const FString& Key, FString& OutValue) const
{
	const FSteamId Lobby = GetActiveLobbyId();
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
	{
		OutValue.Reset();
		return false;
	}
	return LobbyBackend->GetLobbyData(Lobby, Key, OutValue);
}

TMap<FString, FString> USteamSessionsSubsystem::GetAllLobbyData() const
{
	TMap<FString, FString> Data;
	const FSteamId Lobby = GetActiveLobbyId();
	if (LobbyBackend.IsValid() && Lobby.IsValid())
	{
		LobbyBackend->GetAllLobbyData(Lobby, Data);
	}
	return Data;
}

FString USteamSessionsSubsystem::GetMemberData(FSteamId Member, const FString& Key) const
{
	const FSteamId Lobby = GetActiveLobbyId();
	return (LobbyBackend.IsValid() && Lobby.IsValid()) ? LobbyBackend->GetMemberData(Lobby, Member, Key) : FString();
}

bool USteamSessionsSubsystem::AreAllMembersReady() const
{
	const TArray<FSteamLobbyMember> Members = GetLobbyMembers();
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

FSteamResult USteamSessionsSubsystem::SetLobbyData(const FString& Key, const FString& Value)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateLobbyKey(Key, Error) || !SandwichSteam::Sessions::ValidateLobbyValue(Value, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->SetLobbyData(Lobby, Key, Value)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbySetDataFailed", "Steam refused to set the lobby data."));
}

FSteamResult USteamSessionsSubsystem::RemoveLobbyData(const FString& Key)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateLobbyKey(Key, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->DeleteLobbyData(Lobby, Key)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyRemoveDataFailed", "Steam refused to remove the lobby data."));
}

FSteamResult USteamSessionsSubsystem::SetLobbyJoinable(bool bJoinable)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->SetJoinable(Lobby, bJoinable)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyJoinableFailed", "Steam refused to change whether the lobby can be joined."));
}

FSteamResult USteamSessionsSubsystem::SetMemberData(const FString& Key, const FString& Value)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateLobbyKey(Key, Error) || !SandwichSteam::Sessions::ValidateLobbyValue(Value, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobby(Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	LobbyBackend->SetMemberData(Lobby, Key, Value);
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::SetReady(bool bReady)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobby(Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	LobbyBackend->SetMemberData(Lobby, SandwichSteam::Sessions::ReadyKey(), bReady ? TEXT("1") : TEXT("0"));
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::SendChatMessage(const FString& Text)
{
	FText Error;
	if (!SandwichSteam::Sessions::ValidateChatText(Text, Error))
	{
		return MakeArgumentError(Error);
	}

	FSteamId Lobby;
	const FSteamResult Result = RequireLobby(Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return LobbyBackend->SendChat(Lobby, Text)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyChatFailed", "Steam could not send the chat message."));
}

// ---- Kick ----

FSteamResult USteamSessionsSubsystem::KickMember(FSteamId Member, const FString& Reason)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Lobby);
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

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: kicked %s from lobby %s"), *Member.ToString(), *Lobby.ToString());
	if (IsInSession())
	{
		KickFromGameSession(Member, FText::FromString(Reason));
	}
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::ForgiveMember(FSteamId Member)
{
	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(Lobby);
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

bool USteamSessionsSubsystem::CheckKicked()
{
	const FSteamId Lobby = GetActiveLobbyId();
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
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

	const FString Reason = Marker == TEXT("1") ? FString() : Marker;
	UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: the lobby owner kicked this player from lobby %s%s%s"), *Lobby.ToString(), Reason.IsEmpty() ? TEXT("") : TEXT(": "), *Reason);

	const bool bLobbyOnly = LobbyOnlyId.IsValid();
	if (bLobbyOnly)
	{
		LobbyBackend->LeaveLobby(Lobby);
		LobbyOnlyId = FSteamId();
		ActiveProfile = FGameplayTag();
		HostedSettings = FSteamSessionSettings();
		RefreshLobbyOwner();
	}

	OnLobbyKicked.Broadcast(Reason);

	if (!bLobbyOnly)
	{
		// A session lobby: ending the session leaves the lobby, and OnSessionLost tells the game to go back to the menu.
		HandleSessionLost(FGameplayTag(SteamGameplayTags::Error_Session_Kicked), Reason.IsEmpty() ? FString(TEXT("Kicked by the lobby owner")) : Reason);
	}

	return true;
}

// ---- Steam callbacks (game thread, through the dispatcher) ----

void USteamSessionsSubsystem::RefreshLobbyOwner()
{
	const FSteamId Lobby = GetActiveLobbyId();
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
	{
		TrackedLobby = FSteamId();
		TrackedOwner = FSteamId();
		return;
	}

	const FSteamId Owner = LobbyBackend->GetOwner(Lobby);
	if (Lobby != TrackedLobby)
	{
		// A new lobby: remember its owner, no event.
		TrackedLobby = Lobby;
		TrackedOwner = Owner;
		return;
	}

	if (Owner.IsValid() && Owner != TrackedOwner)
	{
		const FSteamId Previous = TrackedOwner;
		TrackedOwner = Owner;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby owner changed from %s to %s"), *Previous.ToString(), *Owner.ToString());
		OnLobbyOwnerChanged.Broadcast(Owner, Previous);
	}
}

void USteamSessionsSubsystem::HandleLobbyDataUpdate(FSteamId Lobby, FSteamId Member, bool bSuccess)
{
	if (!bSuccess || !LobbyBackend.IsValid() || Lobby != GetActiveLobbyId())
	{
		return;
	}

	if (Member == Lobby)
	{
		// Data of the lobby itself.
		RefreshLobbyOwner();
		if (CheckKicked())
		{
			return;
		}
		OnLobbyDataChanged.Broadcast();
	}
	else
	{
		OnLobbyMemberDataChanged.Broadcast(MakeLobbyMember(Lobby, Member, LobbyBackend->GetOwner(Lobby)));
	}
}

void USteamSessionsSubsystem::HandleLobbyChatUpdate(FSteamId Lobby, FSteamId Changed, uint32 Flags)
{
	if (!LobbyBackend.IsValid() || Lobby != GetActiveLobbyId())
	{
		return;
	}

	const ESteamLobbyMemberChange Change = SandwichSteam::Sessions::MemberChangeFromFlags(Flags);
	FSteamLobbyMember Member = MakeLobbyMember(Lobby, Changed, LobbyBackend->GetOwner(Lobby));
	if (Change != ESteamLobbyMemberChange::Entered)
	{
		Member.bReady = false; // Gone: its data is not readable any more.
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby member %s %s (change %d)"), *Changed.ToString(), *Member.Name, static_cast<int32>(Change));
	OnLobbyMemberChanged.Broadcast(Member, Change);

	// Steam gives the lobby to another member when the owner leaves; that shows up here first.
	RefreshLobbyOwner();
}

void USteamSessionsSubsystem::HandleLobbyChatMessage(FSteamId Lobby, FSteamId Sender, const FString& Text)
{
	if (!LobbyBackend.IsValid() || Lobby != GetActiveLobbyId())
	{
		return;
	}

	++ChatReceivedCount;
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby chat from %s: %s"), *Sender.ToString(), *Text);

	FSteamLobbyChatMessage Message;
	Message.Sender = Sender;
	Message.SenderName = LobbyBackend->GetMemberName(Sender);
	Message.Text = Text;
	Message.bIsLocal = Sender == LobbyBackend->GetLocalId();
	OnLobbyChatReceived.Broadcast(Message);
}
