// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamSessionsBackend.h"
#include "Core/SteamBackend.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamSDK.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "SteamSessionRules.h"
#include "SteamSessionsSubsystem.h"

// Raw Steam callbacks of the Sessions feature. Steam calls them on its callback thread: copy the payload, dispatch, return.
class FSteamSessionsRawCallbacks
{
public:
	FSteamSessionsRawCallbacks(USteamSessionsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
		: Owner(InOwner)
		, Dispatcher(InDispatcher)
	{
#if SANDWICHSTEAM_WITH_STEAMWORKS
		JoinRequestedCallback.Register(this, &FSteamSessionsRawCallbacks::OnGameRichPresenceJoinRequested);
#endif
	}

	FSteamSessionsRawCallbacks(const FSteamSessionsRawCallbacks&) = delete;
	FSteamSessionsRawCallbacks& operator=(const FSteamSessionsRawCallbacks&) = delete;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	/** A friend clicked "Join Game" while this game is running: Steam hands over the connect string of the rich presence. */
	STEAM_CALLBACK_MANUAL(FSteamSessionsRawCallbacks, OnGameRichPresenceJoinRequested, GameRichPresenceJoinRequested_t, JoinRequestedCallback);
#endif

	TWeakObjectPtr<USteamSessionsSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};

#if SANDWICHSTEAM_WITH_STEAMWORKS
void FSteamSessionsRawCallbacks::OnGameRichPresenceJoinRequested(GameRichPresenceJoinRequested_t* Payload)
{
	const FString Connect = UTF8_TO_TCHAR(Payload->m_rgchConnect);
	const FSteamId Friend(static_cast<int64>(Payload->m_steamIDFriend.ConvertToUint64()));
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [Connect, Friend](USteamSessionsSubsystem& Sessions)
	{
		FSteamJoinIntent Intent;
		if (SandwichSteam::Sessions::ParseConnectString(Connect, Intent))
		{
			Sessions.HandleConnectRequest(Intent, Friend);
		}
		else
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: Join Game asked for the connect string '%s', which has neither +connect_lobby nor +connect. Set the connect string of the rich presence to '+connect_lobby <lobby id>'."), *Connect);
		}
	});
}
#endif

namespace
{
	FSteamResult MapJoinResult(EOnJoinSessionCompleteResult::Type Result)
	{
		const int32 Native = static_cast<int32>(Result);
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success:
			return FSteamResult::Success();
		case EOnJoinSessionCompleteResult::SessionIsFull:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_Full, NSLOCTEXT("SandwichSteam", "JoinFull", "The session is full."), Native);
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound, NSLOCTEXT("SandwichSteam", "JoinNotFound", "The session does not exist any more."), Native);
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress:
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "JoinNoAddress", "Steam joined the lobby but could not get the host address."), Native);
		case EOnJoinSessionCompleteResult::AlreadyInSession:
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "JoinAlready", "Already in a session. Destroy it first."), Native);
		default:
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "JoinUnknown", "Steam could not join the session."), Native);
		}
	}

#if SANDWICHSTEAM_WITH_STEAMWORKS
	ELobbyDistanceFilter ToSteamDistance(ESteamSessionDistance Distance)
	{
		switch (Distance)
		{
		case ESteamSessionDistance::Close: return k_ELobbyDistanceFilterClose;
		case ESteamSessionDistance::Far: return k_ELobbyDistanceFilterFar;
		case ESteamSessionDistance::Worldwide: return k_ELobbyDistanceFilterWorldwide;
		default: return k_ELobbyDistanceFilterDefault;
		}
	}
#endif
}

FSteamSessionsBackend::FSteamSessionsBackend(USteamSessionsSubsystem* InOwner, UGameInstance* InGameInstance, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, GameInstance(InGameInstance)
	, Dispatcher(InDispatcher)
{
	IOnlineSubsystem* OSS = SandwichSteam::GetSteamOSS(InGameInstance);
	if (!OSS)
	{
		return;
	}

	Sessions = OSS->GetSessionInterface();
	LocalUserId = GetLocalUserNetId(*OSS);
	if (!Sessions.IsValid())
	{
		return;
	}

	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateRaw(this, &FSteamSessionsBackend::OnCreateSessionComplete));
	UpdateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateRaw(this, &FSteamSessionsBackend::OnUpdateSessionComplete));
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateRaw(this, &FSteamSessionsBackend::OnDestroySessionComplete));
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateRaw(this, &FSteamSessionsBackend::OnFindSessionsComplete));
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateRaw(this, &FSteamSessionsBackend::OnJoinSessionComplete));
	InviteHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateRaw(this, &FSteamSessionsBackend::OnSessionUserInviteAccepted));

	// Dedicated servers have no Steam client, so no friends and no Join Game.
	if (!IsRunningDedicatedServer())
	{
		RawCallbacks = MakeUnique<FSteamSessionsRawCallbacks>(InOwner, InDispatcher);
	}
}

FSteamSessionsBackend::~FSteamSessionsBackend()
{
	RawCallbacks.Reset();

	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
	}
}

FUniqueNetIdPtr FSteamSessionsBackend::GetLocalUserNetId(IOnlineSubsystem& OSS)
{
	const IOnlineIdentityPtr Identity = OSS.GetIdentityInterface();
	if (!Identity.IsValid())
	{
		return nullptr;
	}

	FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
#if SANDWICHSTEAM_WITH_STEAMWORKS
	// The identity may not have logged in yet; Steam already knows who the user is.
	if (!UserId.IsValid() && SteamUser())
	{
		UserId = Identity->CreateUniquePlayerId(FString::Printf(TEXT("%llu"), SteamUser()->GetSteamID().ConvertToUint64()));
	}
#endif
	return UserId;
}

FSteamId FSteamSessionsBackend::GetLocalSteamId() const
{
	FSteamId Id;
	if (LocalUserId.IsValid())
	{
		FSteamId::FromUniqueNetId(*LocalUserId, Id);
	}
	return Id;
}

bool FSteamSessionsBackend::CreateSession(const FGameplayTag& ProfileTag, const FSteamSessionSettings& InSettings)
{
	if (!IsUsable())
	{
		return false;
	}

	const SandwichSteam::Sessions::FSessionShape Shape = SandwichSteam::Sessions::MakeShape(InSettings.Visibility, InSettings.MaxPlayers, InSettings.bUsesPresence, InSettings.bAllowJoinInProgress);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = Shape.PublicConnections;
	Settings.NumPrivateConnections = Shape.PrivateConnections;
	Settings.bShouldAdvertise = Shape.bShouldAdvertise;
	Settings.bAllowJoinInProgress = Shape.bAllowJoinInProgress;
	Settings.bAllowInvites = Shape.bAllowInvites;
	Settings.bUsesPresence = Shape.bUsesPresence;
	Settings.bAllowJoinViaPresence = Shape.bAllowJoinViaPresence;
	Settings.bAllowJoinViaPresenceFriendsOnly = Shape.bAllowJoinViaPresenceFriendsOnly;
	Settings.bUseLobbiesIfAvailable = Shape.bUseLobbies;
	Settings.bUseLobbiesVoiceChatIfAvailable = false; // Voice (Voice module) runs over the game connection; lobby voice is out of scope.
	Settings.bIsLANMatch = false;
	Settings.bIsDedicated = false;
	Settings.bAntiCheatProtected = false;

	if (ProfileTag.IsValid())
	{
		Settings.Set(FName(SandwichSteam::Sessions::ProfileKey()), ProfileTag.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}
	Settings.Set(FName(SandwichSteam::Sessions::NameKey()), InSettings.DisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	for (const TPair<FName, FString>& Pair : InSettings.Settings)
	{
		if (!Pair.Key.IsNone())
		{
			Settings.Set(Pair.Key, Pair.Value, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		}
	}

	if (IsRunningDedicatedServer())
	{
		// Dormant: dedicated servers are not supported (future content, Documents/Plans/Improvements.md 4.1).
		// A dedicated server has no Steam user and no lobby: it registers a game server session.
		Settings.bIsDedicated = true;
		Settings.bUsesPresence = false;
		Settings.bUseLobbiesIfAvailable = false;
		Settings.bShouldAdvertise = InSettings.Visibility == ESteamSessionVisibility::Public;
		Settings.bAllowJoinViaPresence = false;
		Settings.bAllowJoinViaPresenceFriendsOnly = false;
		return Sessions->CreateSession(0, NAME_GameSession, Settings);
	}

	if (!Shape.bUsesPresence)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: creating a session without presence. It is not found by Find Steam Sessions, and invites and Join Game do not work. Only for games that connect by their own address."));
	}

	return LocalUserId.IsValid() && Sessions->CreateSession(*LocalUserId, NAME_GameSession, Settings);
}

bool FSteamSessionsBackend::UpdateSession(const FSteamSessionSettings& InSettings)
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session)
	{
		return false;
	}

	const SandwichSteam::Sessions::FSessionShape Shape = SandwichSteam::Sessions::MakeShape(InSettings.Visibility, InSettings.MaxPlayers, InSettings.bUsesPresence, InSettings.bAllowJoinInProgress);

	// Start from the live settings so keys this call does not touch (the profile tag, anything Steam itself wrote) survive.
	FOnlineSessionSettings Settings = Session->SessionSettings;
	Settings.NumPublicConnections = Shape.PublicConnections;
	Settings.NumPrivateConnections = Shape.PrivateConnections;
	Settings.bShouldAdvertise = Shape.bShouldAdvertise;
	Settings.bAllowJoinInProgress = Shape.bAllowJoinInProgress;
	Settings.bAllowJoinViaPresence = Shape.bAllowJoinViaPresence;
	Settings.bAllowJoinViaPresenceFriendsOnly = Shape.bAllowJoinViaPresenceFriendsOnly;

	Settings.Set(FName(SandwichSteam::Sessions::NameKey()), InSettings.DisplayName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	for (const TPair<FName, FString>& Pair : InSettings.Settings)
	{
		if (!Pair.Key.IsNone())
		{
			Settings.Set(Pair.Key, Pair.Value, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		}
	}

	// [verify] The Steam Online Subsystem's UpdateSession pushes the lobby type, member limit and lobby data to Steam.
	return Sessions.IsValid() && Sessions->UpdateSession(NAME_GameSession, Settings, /*bShouldRefreshOnlineData*/ true);
}

FSteamSessionSettings FSteamSessionsBackend::GetCurrentSettings() const
{
	FSteamSessionSettings Result;
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session)
	{
		return Result;
	}

	const FOnlineSessionSettings& Settings = Session->SessionSettings;
	Result.MaxPlayers = Settings.NumPublicConnections + Settings.NumPrivateConnections;
	Result.bAllowJoinInProgress = Settings.bAllowJoinInProgress;
	Result.bUsesPresence = Settings.bUsesPresence;
	Result.Visibility = (Settings.NumPrivateConnections > 0 && Settings.NumPublicConnections == 0) ? ESteamSessionVisibility::Private
		: (Settings.bShouldAdvertise ? ESteamSessionVisibility::Public : ESteamSessionVisibility::FriendsOnly);

	for (const TPair<FName, FOnlineSessionSetting>& Setting : Settings.Settings)
	{
		const FString Key = Setting.Key.ToString();
		if (Key.Equals(SandwichSteam::Sessions::NameKey(), ESearchCase::IgnoreCase))
		{
			Result.DisplayName = Setting.Value.Data.ToString();
		}
		else if (!Key.Equals(SandwichSteam::Sessions::ProfileKey(), ESearchCase::IgnoreCase))
		{
			Result.Settings.Add(Setting.Key, Setting.Value.Data.ToString());
		}
	}
	return Result;
}

FGameplayTag FSteamSessionsBackend::GetCurrentProfileTag() const
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session)
	{
		return FGameplayTag();
	}

	FString Value;
	if (const FOnlineSessionSetting* Setting = Session->SessionSettings.Settings.Find(FName(SandwichSteam::Sessions::ProfileKey())))
	{
		Value = Setting->Data.ToString();
	}
	return Value.IsEmpty() ? FGameplayTag() : UGameplayTagsManager::Get().RequestGameplayTag(FName(*Value), /*ErrorIfNotFound*/ false);
}

bool FSteamSessionsBackend::IsHost() const
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session)
	{
		return false;
	}

	// 2026-09-27: FNamedOnlineSession::bHosting was not reliably true for a session this game created (a single-machine
	// host reported Steam.Error.NotOwner on Update Steam Session). Compare the owning user instead, set by CreateSession
	// and unaffected by that: a joined client sees the host as the owner, the host sees itself.
	if (Session->OwningUserId.IsValid())
	{
		FSteamId OwnerId;
		FSteamId::FromUniqueNetId(*Session->OwningUserId, OwnerId);
		return OwnerId.IsValid() && OwnerId == GetLocalSteamId();
	}

	return Session->bHosting;
}

int32 FSteamSessionsBackend::GetCurrentPlayerCount() const
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session)
	{
		return 0;
	}

	const int32 Total = Session->SessionSettings.NumPublicConnections + Session->SessionSettings.NumPrivateConnections;
	const int32 Open = Session->NumOpenPublicConnections + Session->NumOpenPrivateConnections;
	return FMath::Max(Total - Open, 0);
}

bool FSteamSessionsBackend::DestroySession()
{
	return Sessions.IsValid() && Sessions->DestroySession(NAME_GameSession);
}

bool FSteamSessionsBackend::FindSessions(const FSteamSessionSearchOptions& Options, int32 KeepHandle)
{
	if (!HasLocalUser())
	{
		return false;
	}

	for (auto It = Results.CreateIterator(); It; ++It)
	{
		if (It.Key() != KeepHandle)
		{
			It.RemoveCurrent();
		}
	}

	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = FMath::Clamp(Options.MaxResults, 1, 50);
	Search->bIsLanQuery = false;
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	if (Options.ProfileTag.IsValid())
	{
		Search->QuerySettings.Set(FName(SandwichSteam::Sessions::ProfileKey()), Options.ProfileTag.ToString(), EOnlineComparisonOp::Equals);
	}

	for (const TPair<FName, FString>& Filter : Options.Filters)
	{
		if (!Filter.Key.IsNone())
		{
			Search->QuerySettings.Set(Filter.Key, Filter.Value, EOnlineComparisonOp::Equals);
		}
	}

#if SANDWICHSTEAM_WITH_STEAMWORKS
	// [verify] Steam keeps this filter until the next lobby list request, which the Online Subsystem makes right after this call.
	if (ISteamMatchmaking* Matchmaking = SteamMatchmaking())
	{
		Matchmaking->AddRequestLobbyListDistanceFilter(ToSteamDistance(Options.Distance));
	}
#endif

	return Sessions->FindSessions(*LocalUserId, Search.ToSharedRef());
}

bool FSteamSessionsBackend::FindSessionById(FSteamId LobbyId)
{
	if (!HasLocalUser() || !LobbyId.IsValid())
	{
		return false;
	}

	const UGameInstance* Instance = GameInstance.Get();
	const TSharedPtr<const FUniqueNetId> LobbyNetId = LobbyId.ToUniqueNetId(Instance);
	if (!LobbyNetId.IsValid())
	{
		return false;
	}

	// [verify] The Steam Online Subsystem resolves a lobby ID passed as the session ID (join by +connect_lobby).
	return Sessions->FindSessionById(*LocalUserId, *LobbyNetId, *LocalUserId,
		FOnSingleSessionResultCompleteDelegate::CreateSP(this, &FSteamSessionsBackend::OnFindSessionByIdComplete));
}

bool FSteamSessionsBackend::JoinSession(int32 ResultHandle)
{
	const FOnlineSessionSearchResult* Result = Results.Find(ResultHandle);
	if (!HasLocalUser() || !Result)
	{
		return false;
	}

	return Sessions->JoinSession(*LocalUserId, NAME_GameSession, *Result);
}

bool FSteamSessionsBackend::IsInSession() const
{
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

FSteamId FSteamSessionsBackend::GetCurrentLobbyId() const
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	FSteamId CurrentId;
	if (Session)
	{
		// [verify] The session ID of a Steam lobby is its decimal SteamID64.
		FSteamId::FromString(Session->GetSessionIdStr(), CurrentId);
	}
	return CurrentId;
}

bool FSteamSessionsBackend::IsInLobby(FSteamId LobbyId) const
{
	return LobbyId.IsValid() && GetCurrentLobbyId() == LobbyId;
}

bool FSteamSessionsBackend::GetConnectString(FString& OutConnect) const
{
	return Sessions.IsValid() && Sessions->GetResolvedConnectString(NAME_GameSession, OutConnect) && !OutConnect.IsEmpty();
}

bool FSteamSessionsBackend::SendInvite(FSteamId Friend) const
{
	if (!HasLocalUser() || !Friend.IsValid())
	{
		return false;
	}

	const TSharedPtr<const FUniqueNetId> FriendId = Friend.ToUniqueNetId(GameInstance.Get());
	return FriendId.IsValid() && Sessions->SendSessionInviteToFriend(*LocalUserId, NAME_GameSession, *FriendId);
}

bool FSteamSessionsBackend::ShowInviteOverlay() const
{
	IOnlineSubsystem* OSS = SandwichSteam::GetSteamOSS(GameInstance.Get());
	const IOnlineExternalUIPtr ExternalUI = OSS ? OSS->GetExternalUIInterface() : nullptr;
	return ExternalUI.IsValid() && IsInSession() && ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

FString FSteamSessionsBackend::DescribeSession() const
{
	const FNamedOnlineSession* Session = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Session)
	{
		return TEXT("No session.");
	}

	FString Connect;
	GetConnectString(Connect);

	FString Text = FString::Printf(TEXT("State: %s, %s\n"), EOnlineSessionState::ToString(Session->SessionState), Session->bHosting ? TEXT("hosting") : TEXT("joined"));
	Text += FString::Printf(TEXT("Session id: %s, owner: %s\n"), *Session->GetSessionIdStr(), *Session->OwningUserName);
	Text += FString::Printf(TEXT("Slots open: public %d / %d, private %d / %d\n"),
		Session->NumOpenPublicConnections, Session->SessionSettings.NumPublicConnections,
		Session->NumOpenPrivateConnections, Session->SessionSettings.NumPrivateConnections);
	Text += FString::Printf(TEXT("Advertised: %s, presence: %s, lobby: %s, invites: %s, join in progress: %s\n"),
		Session->SessionSettings.bShouldAdvertise ? TEXT("yes") : TEXT("no"),
		Session->SessionSettings.bUsesPresence ? TEXT("yes") : TEXT("no"),
		Session->SessionSettings.bUseLobbiesIfAvailable ? TEXT("yes") : TEXT("no"),
		Session->SessionSettings.bAllowInvites ? TEXT("yes") : TEXT("no"),
		Session->SessionSettings.bAllowJoinInProgress ? TEXT("yes") : TEXT("no"));
	Text += FString::Printf(TEXT("Connect string: %s"), Connect.IsEmpty() ? TEXT("(none)") : *Connect);
	return Text;
}

FSteamSessionResult FSteamSessionsBackend::CacheResult(const FOnlineSessionSearchResult& Source)
{
	FSteamSessionResult Result;
	Result.Handle = NextHandle++;
	Results.Add(Result.Handle, Source);

	const FOnlineSession& Session = Source.Session;
	if (Session.OwningUserId.IsValid())
	{
		FSteamId::FromUniqueNetId(*Session.OwningUserId, Result.OwnerId);
	}
	Result.OwnerName = Session.OwningUserName;
	Result.PingMs = Source.PingInMs;
	Result.MaxPlayers = Session.SessionSettings.NumPublicConnections + Session.SessionSettings.NumPrivateConnections;
	Result.OpenSlots = Session.NumOpenPublicConnections + Session.NumOpenPrivateConnections;

	// [verify] The session ID of a Steam lobby is its decimal SteamID64.
	FSteamId::FromString(Source.GetSessionIdStr(), Result.LobbyId);

	for (const TPair<FName, FOnlineSessionSetting>& Setting : Session.SessionSettings.Settings)
	{
		const FString Key = Setting.Key.ToString();
		const FString Value = Setting.Value.Data.ToString();
		Result.Settings.Add(Key, Value);

		if (Key.Equals(SandwichSteam::Sessions::NameKey(), ESearchCase::IgnoreCase))
		{
			Result.DisplayName = Value;
		}
		else if (Key.Equals(SandwichSteam::Sessions::ProfileKey(), ESearchCase::IgnoreCase))
		{
			Result.ProfileTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Value), /*ErrorIfNotFound*/ false);
		}
	}

	return Result;
}

void FSteamSessionsBackend::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleCreateComplete(bWasSuccessful);
	}
}

void FSteamSessionsBackend::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleUpdateComplete(bWasSuccessful);
	}
}

void FSteamSessionsBackend::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleDestroyComplete(bWasSuccessful);
	}
}

void FSteamSessionsBackend::OnFindSessionsComplete(bool bWasSuccessful)
{
	TArray<FSteamSessionResult> Found;
	if (bWasSuccessful && Search.IsValid())
	{
		const FSteamId Local = GetLocalSteamId();
		for (const FOnlineSessionSearchResult& SearchResult : Search->SearchResults)
		{
			if (!SearchResult.IsValid())
			{
				continue;
			}

			FSteamSessionResult Result = CacheResult(SearchResult);
			if (Local.IsValid() && Result.OwnerId == Local)
			{
				Results.Remove(Result.Handle); // Our own lobby is not a match to join.
				continue;
			}
			Found.Add(MoveTemp(Result));
		}
	}
	Search.Reset();

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleFindComplete(bWasSuccessful, Found);
	}
}

void FSteamSessionsBackend::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	FSteamResult Outcome = MapJoinResult(Result);
	FString Connect;
	if (Outcome.IsSuccess() && !GetConnectString(Connect))
	{
		Outcome = FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "JoinNoConnect", "Joined the session but Steam gave no connect string."));
	}

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleJoinComplete(Outcome, Connect);
	}
}

void FSteamSessionsBackend::OnSessionUserInviteAccepted(bool bWasSuccessful, int32 /*ControllerId*/, FUniqueNetIdPtr /*UserId*/, const FOnlineSessionSearchResult& InviteResult)
{
	FSteamSessionResult Result;
	if (bWasSuccessful && InviteResult.IsValid())
	{
		Result = CacheResult(InviteResult);
	}

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleInviteAccepted(bWasSuccessful && Result.IsValid(), Result);
	}
}

void FSteamSessionsBackend::OnFindSessionByIdComplete(int32 /*LocalUserNum*/, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult)
{
	FSteamSessionResult Result;
	if (bWasSuccessful && SearchResult.IsValid())
	{
		Result = CacheResult(SearchResult);
	}

	if (USteamSessionsSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->HandleFindByIdComplete(bWasSuccessful && Result.IsValid(), Result);
	}
}
