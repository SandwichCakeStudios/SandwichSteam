// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

// Party match and party join routing of USteamSessionsSubsystem (Phase 17). The party owner tells the members where the match is
// through reserved lobby data (OSTINGAME, OSTMATCH) and, for a server, SetLobbyGameServer; members follow and stay in the party.
// Join requests (invite, Join Game, launch) for a lobby marked OSTPARTY join the party instead of a net session.

#include "SteamSessionsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "SteamLobbyBackend.h"
#include "SteamSessionRules.h"
#include "SteamSessionsSettings.h"

namespace
{
	/** Seconds to wait for the lobby data of a requested lobby before it is treated as a session lobby. */
	constexpr float PartyLookupTimeoutSeconds = 5.0f;

	const TCHAR* ToText(SandwichSteam::Sessions::EPartyFollow Follow)
	{
		using SandwichSteam::Sessions::EPartyFollow;
		switch (Follow)
		{
		case EPartyFollow::Session: return TEXT("join the session lobby");
		case EPartyFollow::SteamId: return TEXT("travel to the server Steam ID");
		case EPartyFollow::Address: return TEXT("travel to the address");
		default: return TEXT("nothing to do (already there)");
		}
	}
}

// ---- Owner side ----

FSteamResult USteamSessionsSubsystem::CheckPartyOwner() const
{
	FSteamId Lobby;
	return RequireLobbyOwner(ESteamLobbyTarget::Party, Lobby);
}

FSteamResult USteamSessionsSubsystem::BeginPartyMatch(const FSteamPartyMatch& Match)
{
	using namespace SandwichSteam::Sessions;

	FSteamId Lobby;
	const FSteamResult Result = RequireLobbyOwner(ESteamLobbyTarget::Party, Lobby);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const FString Code = EncodePartyMatch(Match);
	if (Code.IsEmpty())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "PartyMatchEmpty", "The party match needs a Session Lobby, a Server Steam Id or an Address (without ';' or '=')."));
	}

	// The state before the first Begin is what End restores; a second Begin during a match keeps it.
	FPartyLobbyState Before;
	FString Saved;
	if (!(LobbyBackend->GetLobbyData(Lobby, VisibilityKey(), Saved) && DecodeSavedLobbyState(Saved, Before)))
	{
		Before.Type = ToPartyLobbyType(PartySettings.Visibility);
		Before.bJoinable = bPartyJoinable;
	}

	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	const FPartyLobbyState During = MakeMatchLobbyState(Before,
		Settings ? Settings->PartyMatchVisibility : ESteamPartyMatchVisibility::Private,
		Settings && Settings->bClosePartyDuringMatch);

	// The match first, then the flag: a member that sees OSTINGAME = 1 can read OSTMATCH.
	bool bOk = LobbyBackend->SetLobbyData(Lobby, VisibilityKey(), EncodeSavedLobbyState(Before));
	bOk &= LobbyBackend->SetLobbyData(Lobby, MatchKey(), Code);
	bOk &= LobbyBackend->SetLobbyData(Lobby, InGameKey(), TEXT("1"));

	uint32 Ip = 0;
	uint16 Port = 0;
	const bool bHasIp = ParseIPv4Address(Match.Address, Ip, Port);
	if (Match.ServerSteamId.IsValid() || bHasIp)
	{
		bOk &= LobbyBackend->SetLobbyGameServer(Lobby, Ip, Port, Match.ServerSteamId);
	}

	if (During.Type != Before.Type)
	{
		bOk &= LobbyBackend->SetPartyLobbyType(Lobby, During.Type);
	}
	if (During.bJoinable != Before.bJoinable)
	{
		bOk &= LobbyBackend->SetJoinable(Lobby, During.bJoinable);
		bPartyJoinable = During.bJoinable;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: party match begun in lobby %s: %s (game server published: %s)"), *Lobby.ToString(), *Code,
		(Match.ServerSteamId.IsValid() || bHasIp) ? TEXT("yes") : TEXT("no"));
	return bOk
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "PartyMatchBeginFailed", "Steam refused one of the party match calls (lobby data, game server, lobby type)."));
}

FSteamResult USteamSessionsSubsystem::EndPartyMatch()
{
	const FSteamResult Result = CheckPartyOwner();
	return Result.IsSuccess() ? EndPartyMatchInternal() : Result;
}

FSteamResult USteamSessionsSubsystem::EndPartyMatchInternal()
{
	using namespace SandwichSteam::Sessions;

	const FSteamId Lobby = PartyLobbyId;
	if (!LobbyBackend.IsValid() || !Lobby.IsValid())
	{
		return FSteamResult::Success();
	}

	FString Saved;
	FPartyLobbyState Before;
	const bool bHasSaved = LobbyBackend->GetLobbyData(Lobby, VisibilityKey(), Saved) && DecodeSavedLobbyState(Saved, Before);

	bool bOk = LobbyBackend->SetLobbyData(Lobby, InGameKey(), TEXT("0"));
	LobbyBackend->DeleteLobbyData(Lobby, MatchKey());
	LobbyBackend->DeleteLobbyData(Lobby, VisibilityKey());

	if (bHasSaved)
	{
		bOk &= LobbyBackend->SetPartyLobbyType(Lobby, Before.Type);
		bOk &= LobbyBackend->SetJoinable(Lobby, Before.bJoinable);
		bPartyJoinable = Before.bJoinable;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: party match ended in lobby %s (lobby state restored: %s)"), *Lobby.ToString(), bHasSaved ? *Saved : TEXT("nothing saved"));
	return bOk
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "PartyMatchEndFailed", "Steam refused one of the calls that end the party match."));
}

FSteamResult USteamSessionsSubsystem::FinishHostPartySession(const FSteamResult& Created)
{
	if (!Created.IsSuccess())
	{
		return Created;
	}

	// Members always reach the host by its Steam ID: the Steam Online Subsystem cannot look a lobby up by its ID
	// (FindSessionById is a stub that fails), so the session lobby only names the match (rejoin, "already there").
	FSteamPartyMatch Match;
	Match.SessionLobby = GetSessionLobbyId();
	if (LobbyBackend.IsValid())
	{
		Match.ServerSteamId = LobbyBackend->GetLocalId();
	}

	const FSteamResult Begun = BeginPartyMatch(Match);
	if (!Begun.IsSuccess())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: the party session was created, but the party match could not begin: %s"), *Begun.Message.ToString());
	}
	return Begun;
}

FSteamResult USteamSessionsSubsystem::HostPartySession(const FGameplayTag& ProfileTag, FSteamSessionOpDelegate OnComplete)
{
	const FSteamResult Owner = CheckPartyOwner();
	if (!Owner.IsSuccess())
	{
		return Owner;
	}

	return CreateSession(ProfileTag, FSteamSessionOpDelegate::CreateWeakLambda(this, [this, OnComplete](const FSteamResult& Done)
	{
		OnComplete.ExecuteIfBound(FinishHostPartySession(Done));
	}));
}

FSteamResult USteamSessionsSubsystem::HostPartySession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete)
{
	const FSteamResult Owner = CheckPartyOwner();
	if (!Owner.IsSuccess())
	{
		return Owner;
	}

	return CreateSession(Settings, FSteamSessionSettingsOpDelegate::CreateWeakLambda(this, [this, OnComplete](const FSteamResult& Done, const FSteamSessionSettings& Applied)
	{
		OnComplete.ExecuteIfBound(FinishHostPartySession(Done), Applied);
	}));
}

FSteamResult USteamSessionsSubsystem::HostPartySession(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete)
{
	const FSteamResult Owner = CheckPartyOwner();
	if (!Owner.IsSuccess())
	{
		return Owner;
	}

	return CreateSession(ProfileTag, Settings, FSteamSessionSettingsOpDelegate::CreateWeakLambda(this, [this, OnComplete](const FSteamResult& Done, const FSteamSessionSettings& Applied)
	{
		OnComplete.ExecuteIfBound(FinishHostPartySession(Done), Applied);
	}));
}

// ---- Reading the match ----

bool USteamSessionsSubsystem::IsPartyInMatch() const
{
	FString InGame;
	return GetLobbyData(SandwichSteam::Sessions::InGameKey(), InGame, ESteamLobbyTarget::Party) && SandwichSteam::Sessions::ParseFlag(InGame);
}

bool USteamSessionsSubsystem::GetPartyMatch(FSteamPartyMatch& OutMatch) const
{
	OutMatch = FSteamPartyMatch();
	FString Code;
	return IsPartyInMatch()
		&& GetLobbyData(SandwichSteam::Sessions::MatchKey(), Code, ESteamLobbyTarget::Party)
		&& SandwichSteam::Sessions::DecodePartyMatch(Code, OutMatch);
}

// ---- Member side ----

void USteamSessionsSubsystem::RefreshPartyMatch()
{
	if (!PartyLobbyId.IsValid())
	{
		return;
	}

	if (!IsPartyInMatch())
	{
		if (bPartyMatchActive)
		{
			EndPartyMatchLocal();
		}
		return;
	}

	FString Code;
	FSteamPartyMatch Match;
	if (!GetLobbyData(SandwichSteam::Sessions::MatchKey(), Code, ESteamLobbyTarget::Party) || !SandwichSteam::Sessions::DecodePartyMatch(Code, Match))
	{
		return; // The flag came before the match: wait for the next update.
	}

	if (bPartyMatchActive && bPartyMatchFromCallback)
	{
		// Already started from LobbyGameCreated_t: the lobby data describes the same match, no second event.
		PartyMatchCode = Code;
		PartyMatch = Match;
		bPartyMatchFromCallback = false;
		return;
	}

	if (bPartyMatchActive && Code == PartyMatchCode)
	{
		return;
	}

	StartPartyMatchLocal(Match, Code, /*bFromCallback*/ false);
}

void USteamSessionsSubsystem::StartPartyMatchLocal(const FSteamPartyMatch& Match, const FString& Code, bool bFromCallback)
{
	bPartyMatchActive = true;
	bPartyMatchFromCallback = bFromCallback;
	PartyMatchCode = Code;
	PartyMatch = Match;

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: party match started: %s (from %s)"), *Code, bFromCallback ? TEXT("LobbyGameCreated_t") : TEXT("lobby data"));
	OnPartyMatchStarted.Broadcast(Match);

	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	if (PartyLobbyId.IsValid() && bPartyMatchActive && Settings && Settings->bAutoFollowPartyMatch)
	{
		const FSteamResult Followed = FollowMatch(Match);
		if (!Followed.IsSuccess())
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: could not follow the party match: %s"), *Followed.Message.ToString());
		}
	}
}

void USteamSessionsSubsystem::EndPartyMatchLocal()
{
	bPartyMatchActive = false;
	bPartyMatchFromCallback = false;
	PartyMatchCode.Reset();
	PartyMatch = FSteamPartyMatch();

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: party match ended"));
	OnPartyMatchEnded.Broadcast();
}

void USteamSessionsSubsystem::HandleLobbyGameCreated(FSteamId Lobby, FSteamId Server, uint32 Ip, uint16 Port)
{
	if (!PartyLobbyId.IsValid() || Lobby != PartyLobbyId || bPartyMatchActive)
	{
		return; // Not the party, or the lobby data already started this match.
	}

	// Prefer the lobby data (it may carry a session lobby); it often arrives in the same frame.
	FSteamPartyMatch Match;
	if (GetPartyMatch(Match))
	{
		StartPartyMatchLocal(Match, SandwichSteam::Sessions::EncodePartyMatch(Match), /*bFromCallback*/ false);
		return;
	}

	Match.ServerSteamId = Server;
	if (Ip != 0 && Port != 0)
	{
		Match.Address = SandwichSteam::Sessions::FormatIPv4Address(Ip, Port);
	}

	if (Match.IsValid())
	{
		StartPartyMatchLocal(Match, SandwichSteam::Sessions::EncodePartyMatch(Match), /*bFromCallback*/ true);
	}
}

FSteamResult USteamSessionsSubsystem::FollowPartyMatch()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	FSteamPartyMatch Match;
	if (!GetPartyMatch(Match))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "PartyNoMatch", "The party is not in a match (or there is no party)."));
	}

	return FollowMatch(Match);
}

FSteamResult USteamSessionsSubsystem::FollowMatch(const FSteamPartyMatch& Match)
{
	using SandwichSteam::Sessions::EPartyFollow;

	FSteamResult Result = RequireClient();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const EPartyFollow Follow = SandwichSteam::Sessions::DecideFollow(Match, LobbyBackend.IsValid() ? LobbyBackend->GetLocalId() : FSteamId(), GetSessionLobbyId());
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: following the party match: %s"), ToText(Follow));

	switch (Follow)
	{
	case EPartyFollow::Session:
	{
		if (Busy != EBusyOp::None)
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "PartyFollowBusy", "Another session request is still running. Follow the party match when it finished."));
		}

		FSteamJoinRequest Request;
		Request.Source = ESteamJoinSource::Party;
		Request.LobbyId = Match.SessionLobby;
		Request.InviterId = GetLobbyOwnerId(ESteamLobbyTarget::Party);
		BeginSessionLookup(Request);
		return FSteamResult::Success();
	}
	case EPartyFollow::SteamId:
	case EPartyFollow::Address:
	{
		const FString Url = Follow == EPartyFollow::SteamId ? TEXT("steam.") + Match.ServerSteamId.ToString() : Match.Address;
		Result = ClientTravelTo(Url);
		LastConnectString = Url;
		OnSessionJoinFinished.Broadcast(Result, Url);
		return Result;
	}
	default:
		return FSteamResult::Success();
	}
}

// ---- Join requests for party lobbies ----

void USteamSessionsSubsystem::HandleLobbyJoinRequested(FSteamId Lobby, FSteamId Friend)
{
	// The Online Subsystem reports the same request for session lobbies (On Session User Invite Accepted); this path only takes parties.
	FSteamJoinRequest Request;
	Request.Source = ESteamJoinSource::Invite;
	Request.LobbyId = Lobby;
	Request.InviterId = Friend;
	BeginLobbyLookup(Request, /*bSessionFallback*/ false);
}

void USteamSessionsSubsystem::BeginLobbyLookup(FSteamJoinRequest Request, bool bSessionFallback)
{
	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	if (SandwichSteam::Sessions::IsDuplicateRequest(FPlatformTime::Seconds(), LastRequestTime, LastRequestLobby, Request.LobbyId, Settings ? Settings->DuplicateRequestSeconds : 0.0f))
	{
		++DuplicateCount;
		return;
	}

	if (Request.Source == ESteamJoinSource::Party || !LobbyBackend.IsValid() || !LobbyBackend->IsUsable())
	{
		if (bSessionFallback)
		{
			BeginSessionLookup(Request);
		}
		return;
	}

	if (Request.LobbyId.IsValid() && Request.LobbyId == PartyLobbyId)
	{
		Request.bParty = true;
		ProcessPartyRequest(Request);
		return;
	}

	if (PartyLookup.IsSet())
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: a new join request replaces the lookup of lobby %s."), *PartyLookup->LobbyId.ToString());
	}

	PartyLookup = Request;
	bPartyLookupSessionFallback = bSessionFallback;
	if (PartyLookupTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PartyLookupTimeoutHandle);
		PartyLookupTimeoutHandle.Reset();
	}

	if (!LobbyBackend->RequestLobbyData(Request.LobbyId))
	{
		// Steam answers at once for a lobby it already knows (or refuses): read what is cached.
		ResolvePartyLookup(/*bAnswered*/ false);
		return;
	}

	PartyLookupTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		PartyLookupTimeoutHandle.Reset();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: no lobby data arrived in time, treating the lobby as a session lobby."));
		ResolvePartyLookup(/*bAnswered*/ false);
		return false;
	}), PartyLookupTimeoutSeconds);
}

void USteamSessionsSubsystem::ResolvePartyLookup(bool bAnswered)
{
	if (!PartyLookup.IsSet())
	{
		return;
	}

	if (PartyLookupTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PartyLookupTimeoutHandle);
		PartyLookupTimeoutHandle.Reset();
	}

	FSteamJoinRequest Request = *PartyLookup;
	PartyLookup.Reset();
	const bool bSessionFallback = bPartyLookupSessionFallback;

	FString PartyValue;
	const bool bParty = LobbyBackend.IsValid()
		&& LobbyBackend->GetLobbyData(Request.LobbyId, SandwichSteam::Sessions::PartyKey(), PartyValue)
		&& SandwichSteam::Sessions::ParseFlag(PartyValue);
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: lobby %s is %s (lobby data %s)"), *Request.LobbyId.ToString(), bParty ? TEXT("a party lobby") : TEXT("not a party lobby"),
		bAnswered ? TEXT("arrived") : TEXT("read from the cache"));

	if (bParty)
	{
		Request.bParty = true;
		LastPartyRequestTime = FPlatformTime::Seconds();
		ProcessPartyRequest(Request);
	}
	else if (bSessionFallback)
	{
		BeginSessionLookup(Request);
	}
}

void USteamSessionsSubsystem::ProcessPartyRequest(FSteamJoinRequest Request)
{
	if (IsRepeatedRequest(Request))
	{
		return;
	}

	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	Request.bParty = true;
	Request.bInMatch = PartyLobbyId.IsValid();
	Request.Action = SandwichSteam::Sessions::DecideJoin(
		Settings ? Settings->JoinWhileInMatch : ESteamJoinInMatchPolicy::AskGame,
		Request.bInMatch,
		Settings ? Settings->bAutoJoinRequests : true,
		Request.LobbyId == PartyLobbyId);

	if ((Request.Action == ESteamJoinAction::JoinNow || Request.Action == ESteamJoinAction::LeaveAndJoin) && Busy != EBusyOp::None)
	{
		Request.Action = ESteamJoinAction::Ignore;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: party join request for %s, in a party: %s, action %d"), *Request.LobbyId.ToString(), Request.bInMatch ? TEXT("yes") : TEXT("no"), static_cast<int32>(Request.Action));

	PendingRequest.Reset();
	if (Request.Action == ESteamJoinAction::AskGame)
	{
		PendingRequest = Request;
	}

	OnJoinRequested.Broadcast(Request);

	if (Request.Action == ESteamJoinAction::JoinNow || Request.Action == ESteamJoinAction::LeaveAndJoin)
	{
		ExecutePartyRequest(Request);
	}
}

void USteamSessionsSubsystem::ExecutePartyRequest(const FSteamJoinRequest& Request)
{
	PendingRequest.Reset();

	if (PartyLobbyId.IsValid() && PartyLobbyId != Request.LobbyId)
	{
		LeaveLobby();
	}

	const FSteamId Lobby = Request.LobbyId;
	const FSteamResult Started = JoinLobby(Lobby, FSteamLobbyOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Done, FSteamId Entered)
	{
		OnPartyJoinFinished.Broadcast(Done, Entered);
	}));

	if (!Started.IsSuccess())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: party join request failed: %s"), *Started.Message.ToString());
		OnPartyJoinFinished.Broadcast(Started, Lobby);
	}
}
