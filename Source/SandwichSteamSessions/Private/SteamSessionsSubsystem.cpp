// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamSessionsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "SteamLobbyBackend.h"
#include "SteamSessionRules.h"
#include "SteamSessionsBackend.h"
#include "SteamSessionsSettings.h"

using SandwichSteam::Sessions::ELostReason;

namespace
{
	FSteamResult MakeBusyResult()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "SessionsBusy", "Another session request is still running. Wait for it to finish."));
	}

	FGameplayTag ToErrorTag(ELostReason Reason)
	{
		switch (Reason)
		{
		case ELostReason::HostLeft: return FGameplayTag(SteamGameplayTags::Error_Session_HostLeft);
		case ELostReason::Timeout: return FGameplayTag(SteamGameplayTags::Error_Session_Timeout);
		case ELostReason::Kicked: return FGameplayTag(SteamGameplayTags::Error_Session_Kicked);
		default: return FGameplayTag(SteamGameplayTags::Error_Failed);
		}
	}

	const TCHAR* ToText(ESteamJoinAction Action)
	{
		switch (Action)
		{
		case ESteamJoinAction::JoinNow: return TEXT("join now");
		case ESteamJoinAction::LeaveAndJoin: return TEXT("leave and join");
		case ESteamJoinAction::AskGame: return TEXT("ask the game");
		default: return TEXT("ignore");
		}
	}

	const TCHAR* ToText(ESteamJoinSource Source)
	{
		switch (Source)
		{
		case ESteamJoinSource::Invite: return TEXT("invite");
		case ESteamJoinSource::Overlay: return TEXT("Join Game");
		default: return TEXT("launch");
		}
	}
}

USteamSessionsSubsystem* USteamSessionsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamSessionsSubsystem>() : nullptr;
}

USteamSessionsSubsystem::USteamSessionsSubsystem() = default;
USteamSessionsSubsystem::~USteamSessionsSubsystem() = default;

FGameplayTag USteamSessionsSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Sessions;
}

bool USteamSessionsSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	Definition = ToolSettings ? ToolSettings->LoadAppDefinition() : nullptr;
	if (!Definition)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: no App Definition assigned in the Sandwich Steam settings. Sessions cannot be created by profile tag."));
	}

	Backend = MakeShared<FSteamSessionsBackend>(this, GetGameInstance(), Dispatcher.ToSharedRef());
	if (!Backend->IsUsable())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: the Steam session interface or the local Steam user is not available. Check that OnlineSubsystemSteam is enabled and configured (Configure Steam)."));
		Backend.Reset();
		Dispatcher.Reset();
		Definition = nullptr;
		return false;
	}

	LobbyBackend = MakeShared<FSteamLobbyBackend>(this, Dispatcher.ToSharedRef());
	LobbyOnlyId = FSteamId();
	TrackedLobby = FSteamId();
	TrackedOwner = FSteamId();

	Busy = EBusyOp::None;
	bFinding = false;
	bFindingLobbies = false;

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &USteamSessionsSubsystem::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &USteamSessionsSubsystem::HandleTravelFailure);
	}
	GameModeInitializedHandle = FGameModeEvents::GameModeInitializedEvent.AddUObject(this, &USteamSessionsSubsystem::HandleGameModeInitialized);

	ScheduleLaunchIntent();
	return true;
#else
	return false;
#endif
}

void USteamSessionsSubsystem::ShutdownFeature()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FGameModeEvents::GameModeInitializedEvent.Remove(GameModeInitializedHandle);
	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();
	GameModeInitializedHandle.Reset();

	if (LaunchIntentHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(LaunchIntentHandle);
		LaunchIntentHandle.Reset();
	}

	// Do not leave a lobby behind when the feature stops (offline, PIE end, quit).
	if (Backend.IsValid() && Backend->IsInSession())
	{
		Backend->DestroySession();
	}

	// Same for a lobby only lobby.
	if (LobbyBackend.IsValid() && LobbyOnlyId.IsValid())
	{
		LobbyBackend->LeaveLobby(LobbyOnlyId);
	}

	PendingCreate.Unbind();
	PendingUpdate.Unbind();
	PendingDestroy.Unbind();
	PendingJoin.Unbind();
	PendingFind.Unbind();
	PendingLobbyJoin.Unbind();
	PendingLobbyCreate.Unbind();
	PendingLobbyFind.Unbind();
	PendingRequest.Reset();
	LookupRequest.Reset();
	Busy = EBusyOp::None;
	bFinding = false;
	bFindingLobbies = false;
	ActiveProfile = FGameplayTag();
	HostedSettings = FSteamSessionSettings();
	LobbyOnlyId = FSteamId();
	TrackedLobby = FSteamId();
	TrackedOwner = FSteamId();

	LobbyBackend.Reset();
	Backend.Reset();
	Dispatcher.Reset();
	Definition = nullptr;
}

FSteamResult USteamSessionsSubsystem::RequireClient() const
{
	if (IsRunningDedicatedServer())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_WrongScope,
			NSLOCTEXT("SandwichSteam", "SessionsClientOnly", "This is a client feature: a dedicated server only hosts sessions."));
	}
	return FSteamResult::Success();
}

bool USteamSessionsSubsystem::IsInSession() const
{
	return Backend.IsValid() && Backend->IsInSession();
}

FSteamId USteamSessionsSubsystem::GetLobbyId() const
{
	return Backend.IsValid() ? Backend->GetCurrentLobbyId() : FSteamId();
}

int32 USteamSessionsSubsystem::GetProtectedHandle() const
{
	return PendingRequest.IsSet() ? PendingRequest->ResultHandle : 0;
}

FSteamResult USteamSessionsSubsystem::CreateSession(const FGameplayTag& ProfileTag, FSteamSessionOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so sessions cannot be created by profile tag."));
	}

	const FSteamSessionProfileDef* Profile = Definition->FindSessionProfile(ProfileTag);
	if (!Profile)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "SessionsUnknownProfile", "The tag '{0}' is not a session profile of the Steam App Definition."), FText::FromName(ProfileTag.GetTagName())));
	}

	FSteamSessionSettings Defaults;
	SandwichSteam::Sessions::MakeDefaultSettings(*Profile, Defaults);
	return StartCreateSession(ProfileTag, Profile, Defaults, FSteamSessionSettingsOpDelegate::CreateWeakLambda(this, [OnComplete](const FSteamResult& Done, const FSteamSessionSettings&)
	{
		OnComplete.ExecuteIfBound(Done);
	}));
}

FSteamResult USteamSessionsSubsystem::CreateSession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	return StartCreateSession(FGameplayTag(), nullptr, Settings, MoveTemp(OnComplete));
}

FSteamResult USteamSessionsSubsystem::CreateSession(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so sessions cannot be created by profile tag."));
	}

	const FSteamSessionProfileDef* Profile = Definition->FindSessionProfile(ProfileTag);
	if (!Profile)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "SessionsUnknownProfile", "The tag '{0}' is not a session profile of the Steam App Definition."), FText::FromName(ProfileTag.GetTagName())));
	}

	return StartCreateSession(ProfileTag, Profile, Settings, MoveTemp(OnComplete));
}

FSteamResult USteamSessionsSubsystem::StartCreateSession(const FGameplayTag& ProfileTag, const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Requested, FSteamSessionSettingsOpDelegate OnComplete)
{
	FSteamSessionSettings Applied;
	FText Error;
	if (!SandwichSteam::Sessions::ResolveSessionSettings(Profile, Requested, Applied, Error))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, Error);
	}

	if (Busy != EBusyOp::None)
	{
		return MakeBusyResult();
	}

	if (IsInSession())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsAlreadyIn", "Already in a session. Destroy it first."));
	}

	if (LobbyOnlyId.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsInLobbyOnly", "Already in a lobby only lobby. Leave it first."));
	}

	Busy = EBusyOp::Creating;
	PendingCreate = OnComplete;
	ActiveProfile = ProfileTag;
	HostedSettings = Applied;
	if (!Backend->CreateSession(ProfileTag, Applied))
	{
		Busy = EBusyOp::None;
		PendingCreate.Unbind();
		ActiveProfile = FGameplayTag();
		HostedSettings = FSteamSessionSettings();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsCreateRefused", "Steam refused to start creating the session."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::UpdateSession(const FSteamSessionSettings& Settings, FSteamSessionSettingsOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!IsInSession() && !LobbyOnlyId.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "SessionsUpdateNoSession", "There is no session or lobby to update."));
	}

	if ((IsInSession() && !Backend->IsHost()) || (LobbyOnlyId.IsValid() && !IsLobbyOwner()))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotOwner, NSLOCTEXT("SandwichSteam", "SessionsUpdateNotHost", "Only the host / lobby owner can update it."));
	}

	const FSteamSessionProfileDef* Profile = (Definition && ActiveProfile.IsValid()) ? Definition->FindSessionProfile(ActiveProfile) : nullptr;
	if (ActiveProfile.IsValid() && !Profile)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsUpdateProfileGone", "The session profile it was created from is no longer in the Steam App Definition."));
	}

	const int32 CurrentPlayers = IsInSession() ? Backend->GetCurrentPlayerCount() : GetLobbyMembers().Num();
	FSteamSessionSettings Applied;
	FText Error;
	if (!SandwichSteam::Sessions::ValidateSessionUpdate(Profile, HostedSettings, Settings, CurrentPlayers, Applied, Error))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, Error);
	}

	if (Busy != EBusyOp::None)
	{
		return MakeBusyResult();
	}

	if (IsInSession())
	{
		Busy = EBusyOp::Updating;
		PendingUpdate = MoveTemp(OnComplete);
		HostedSettings = Applied;
		if (!Backend->UpdateSession(Applied))
		{
			Busy = EBusyOp::None;
			PendingUpdate.Unbind();
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsUpdateRefused", "Steam refused to start updating the session."));
		}
		return FSteamResult::Success();
	}

	// Lobby only mode: raw ISteamMatchmaking calls, all synchronous (no call result to wait for).
	HostedSettings = Applied;
	bool bOk = LobbyBackend->SetLobbyMemberLimit(LobbyOnlyId, Applied.MaxPlayers);
	bOk &= LobbyBackend->SetLobbyType(LobbyOnlyId, Applied.Visibility);
	bOk &= LobbyBackend->SetLobbyData(LobbyOnlyId, SandwichSteam::Sessions::NameKey(), Applied.DisplayName);
	for (const TPair<FName, FString>& Pair : Applied.Settings)
	{
		bOk &= LobbyBackend->SetLobbyData(LobbyOnlyId, Pair.Key.ToString(), Pair.Value);
	}

	const FSteamResult UpdateOutcome = bOk
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsUpdateLobbyFailed", "Steam refused one of the lobby update calls."));
	OnComplete.ExecuteIfBound(UpdateOutcome, Applied);
	return UpdateOutcome;
}

FSteamResult USteamSessionsSubsystem::GetCurrentSettings(FSteamSessionSettings& OutSettings, FGameplayTag& OutProfileTag) const
{
	OutSettings = FSteamSessionSettings();
	OutProfileTag = FGameplayTag();

	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (IsInSession() && Backend.IsValid())
	{
		if (Backend->IsHost())
		{
			// The host resolved these settings itself; a joined client only sees what Steam replicated.
			OutSettings = HostedSettings;
			OutProfileTag = ActiveProfile;
		}
		else
		{
			OutSettings = Backend->GetCurrentSettings();
			OutProfileTag = Backend->GetCurrentProfileTag();
		}
		return FSteamResult::Success();
	}

	if (LobbyOnlyId.IsValid())
	{
		if (IsLobbyOwner())
		{
			OutSettings = HostedSettings;
			OutProfileTag = ActiveProfile;
			return FSteamResult::Success();
		}

		// A member of somebody else's lobby only lobby: reconstruct what Steam can tell (Visibility is not readable back, known limit).
		OutSettings.MaxPlayers = GetLobbyMemberLimit();
		OutSettings.bUsesPresence = true;
		const TMap<FString, FString> Data = GetAllLobbyData();
		for (const TPair<FString, FString>& Pair : Data)
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

	return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "SessionsNoCurrent", "There is no current session or lobby."));
}

FSteamResult USteamSessionsSubsystem::MakeSettingsFromProfile(const FGameplayTag& ProfileTag, FSteamSessionSettings& OutSettings) const
{
	OutSettings = FSteamSessionSettings();
	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so sessions cannot be created by profile tag."));
	}

	const FSteamSessionProfileDef* Profile = Definition->FindSessionProfile(ProfileTag);
	if (!Profile)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "SessionsUnknownProfile", "The tag '{0}' is not a session profile of the Steam App Definition."), FText::FromName(ProfileTag.GetTagName())));
	}

	SandwichSteam::Sessions::MakeDefaultSettings(*Profile, OutSettings);
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::GetProfile(const FGameplayTag& ProfileTag, FSteamSessionProfileDef& OutProfile) const
{
	OutProfile = FSteamSessionProfileDef();
	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so sessions cannot be created by profile tag."));
	}

	const FSteamSessionProfileDef* Profile = Definition->FindSessionProfile(ProfileTag);
	if (!Profile)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "SessionsUnknownProfile", "The tag '{0}' is not a session profile of the Steam App Definition."), FText::FromName(ProfileTag.GetTagName())));
	}

	OutProfile = *Profile;
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::FindSessions(const FSteamSessionSearchOptions& Options, FSteamSessionFindDelegate OnComplete)
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

	if (bFinding)
	{
		return MakeBusyResult();
	}

	bFinding = true;
	PendingFind = OnComplete;
	if (!Backend->FindSessions(Options, GetProtectedHandle()))
	{
		bFinding = false;
		PendingFind.Unbind();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsFindRefused", "Steam refused to start the search."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::JoinSession(const FSteamSessionResult& Result, FSteamSessionJoinDelegate OnComplete)
{
	return StartJoin(Result.Handle, OnComplete);
}

FSteamResult USteamSessionsSubsystem::StartJoin(int32 ResultHandle, FSteamSessionJoinDelegate OnComplete)
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

	if (Busy != EBusyOp::None)
	{
		return MakeBusyResult();
	}

	if (!Backend->HasResult(ResultHandle))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound,
			NSLOCTEXT("SandwichSteam", "SessionsStaleResult", "This search result is out of date. Search again."));
	}

	if (IsInSession())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "SessionsJoinWhileIn", "Already in a session. Destroy it first."));
	}

	if (LobbyOnlyId.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "SessionsJoinWhileLobbyOnly", "Already in a lobby only lobby. Leave it first."));
	}

	Busy = EBusyOp::Joining;
	PendingJoin = OnComplete;
	if (!Backend->JoinSession(ResultHandle))
	{
		Busy = EBusyOp::None;
		PendingJoin.Unbind();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsJoinRefused", "Steam refused to start joining the session."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::DestroySession(FSteamSessionOpDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (Busy != EBusyOp::None)
	{
		return MakeBusyResult();
	}

	if (!IsInSession())
	{
		OnComplete.ExecuteIfBound(FSteamResult::Success());
		return FSteamResult::Success();
	}

	Busy = EBusyOp::Destroying;
	PendingDestroy = OnComplete;
	if (!Backend->DestroySession())
	{
		Busy = EBusyOp::None;
		PendingDestroy.Unbind();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsDestroyRefused", "Steam refused to start closing the session."));
	}

	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::SendInvite(FSteamId Friend)
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

	if (!Friend.IsValid() || !IsInSession())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "SessionsInviteArgs", "Inviting needs a valid friend and a current session."));
	}

	return Backend->SendInvite(Friend)
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsInviteFailed", "Steam could not send the invite."));
}

FSteamResult USteamSessionsSubsystem::ShowInviteOverlay()
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

	if (!IsInSession())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "SessionsOverlayNoSession", "Create or join a session before inviting."));
	}

	return Backend->ShowInviteOverlay()
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsOverlayFailed", "Steam could not open the invite dialog. Is the overlay enabled?"));
}

FSteamResult USteamSessionsSubsystem::ClientTravelTo(const FString& Url)
{
	UGameInstance* Instance = GetGameInstance();
	APlayerController* Controller = Instance ? Instance->GetFirstLocalPlayerController() : nullptr;
	if (!Controller)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsNoController", "There is no local player controller to travel with."));
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: client travel to %s"), *Url);
	Controller->ClientTravel(Url, TRAVEL_Absolute);
	return FSteamResult::Success();
}

FSteamResult USteamSessionsSubsystem::TravelToSession()
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

	FString Connect;
	if (!Backend->GetConnectString(Connect))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "SessionsNoConnect", "There is no session to travel to."));
	}

	return ClientTravelTo(Connect);
}

FSteamResult USteamSessionsSubsystem::ServerTravel(const FString& MapName, bool bListen, bool bAbsolute)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || MapName.IsEmpty())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "SessionsTravelArgs", "Server travel needs a world and a map name."));
	}

	FString Url = MapName;
	if (bListen && !Url.Contains(TEXT("?listen")))
	{
		Url += TEXT("?listen");
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: server travel to %s"), *Url);
	if (World->GetNetMode() == NM_Standalone)
	{
		// Leaving the menu as a host: this is what Open Level with the listen option does.
		GEngine->SetClientTravel(World, *Url, TRAVEL_Absolute);
	}
	else
	{
		World->ServerTravel(Url, bAbsolute);
	}

	return FSteamResult::Success();
}

bool USteamSessionsSubsystem::GetPendingJoinRequest(FSteamJoinRequest& OutRequest) const
{
	if (!PendingRequest.IsSet())
	{
		OutRequest = FSteamJoinRequest();
		return false;
	}

	OutRequest = *PendingRequest;
	return true;
}

FSteamResult USteamSessionsSubsystem::AcceptJoinRequest()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!PendingRequest.IsSet())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "SessionsNoRequest", "No join request is waiting."));
	}

	FSteamJoinRequest Request = *PendingRequest;
	Request.bInMatch = IsInSession();
	Request.Action = Request.bInMatch ? ESteamJoinAction::LeaveAndJoin : ESteamJoinAction::JoinNow;
	ExecuteRequest(Request);
	return FSteamResult::Success();
}

void USteamSessionsSubsystem::DeclineJoinRequest()
{
	PendingRequest.Reset();
}

void USteamSessionsSubsystem::HandleCreateComplete(bool bSuccess)
{
	Busy = EBusyOp::None;

	const FSteamResult Result = bSuccess
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsCreateFailed", "Steam could not create the session."));
	if (!bSuccess)
	{
		ActiveProfile = FGameplayTag();
		HostedSettings = FSteamSessionSettings();
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: create %s"), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	if (bSuccess)
	{
		RefreshLobbyOwner();
		ApplyMaxPlayersToGameSession();
	}

	const FSteamSessionSettingsOpDelegate Done = PendingCreate;
	PendingCreate.Unbind();
	Done.ExecuteIfBound(Result, HostedSettings);
}

void USteamSessionsSubsystem::HandleUpdateComplete(bool bSuccess)
{
	if (Busy == EBusyOp::Updating)
	{
		Busy = EBusyOp::None;
	}

	const FSteamResult Result = bSuccess
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsUpdateFailed", "Steam could not update the session."));
	if (!bSuccess && Backend.IsValid())
	{
		// The optimistic apply in UpdateSession did not stick: fall back to what the session actually has.
		HostedSettings = Backend->GetCurrentSettings();
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: update %s"), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	if (bSuccess)
	{
		ApplyMaxPlayersToGameSession();
	}

	const FSteamSessionSettingsOpDelegate Done = PendingUpdate;
	PendingUpdate.Unbind();
	Done.ExecuteIfBound(Result, HostedSettings);
}

void USteamSessionsSubsystem::HandleDestroyComplete(bool bSuccess)
{
	if (Busy == EBusyOp::Destroying)
	{
		Busy = EBusyOp::None;
	}
	ActiveProfile = FGameplayTag();
	HostedSettings = FSteamSessionSettings();

	const FSteamResult Result = bSuccess
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsDestroyFailed", "Steam could not close the session."));

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: destroy %s"), bSuccess ? TEXT("succeeded") : TEXT("failed"));
	RefreshLobbyOwner(); // Clears the tracked lobby when the session is gone.

	const FSteamSessionOpDelegate Done = PendingDestroy;
	PendingDestroy.Unbind();
	Done.ExecuteIfBound(Result);
}

void USteamSessionsSubsystem::HandleFindComplete(bool bSuccess, const TArray<FSteamSessionResult>& Found)
{
	bFinding = false;

	const FSteamResult Result = bSuccess
		? FSteamResult::Success()
		: FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "SessionsFindFailed", "The session search failed."));

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: search %s, %d result(s)"), bSuccess ? TEXT("finished") : TEXT("failed"), Found.Num());

	const FSteamSessionFindDelegate Done = PendingFind;
	PendingFind.Unbind();
	Done.ExecuteIfBound(Result, Found);
}

void USteamSessionsSubsystem::HandleJoinComplete(const FSteamResult& Result, const FString& ConnectString)
{
	Busy = EBusyOp::None;
	LastConnectString = ConnectString;

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: join %s%s"), Result.IsSuccess() ? TEXT("succeeded") : TEXT("failed: "), Result.IsSuccess() ? TEXT("") : *Result.Message.ToString());

	if (Result.IsSuccess())
	{
		RefreshLobbyOwner();
		const bool bKicked = CheckKicked(); // Joining a lobby whose owner kicked this player before: leave again.

		const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
		if (!bKicked && Settings && Settings->bAutoTravelAfterJoin)
		{
			const FSteamResult Travel = ClientTravelTo(ConnectString);
			if (!Travel.IsSuccess())
			{
				UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: joined, but travel failed: %s"), *Travel.Message.ToString());
			}
		}
	}

	const FSteamSessionJoinDelegate Done = PendingJoin;
	PendingJoin.Unbind();
	Done.ExecuteIfBound(Result, ConnectString);
	OnSessionJoinFinished.Broadcast(Result, ConnectString);
}

void USteamSessionsSubsystem::HandleInviteAccepted(bool bSuccess, const FSteamSessionResult& Result)
{
	if (!bSuccess)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: an invite was accepted but Steam could not resolve the session."));
		OnSessionJoinFinished.Broadcast(FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound,
			NSLOCTEXT("SandwichSteam", "SessionsInviteUnresolved", "The invite could not be resolved. The session may have closed.")), FString());
		return;
	}

	FSteamJoinRequest Request;
	Request.Source = ESteamJoinSource::Invite;
	Request.LobbyId = Result.LobbyId;
	Request.ResultHandle = Result.Handle;
	ProcessRequest(Request);
}

void USteamSessionsSubsystem::HandleConnectRequest(const FSteamJoinIntent& Intent, FSteamId Friend)
{
	FSteamJoinRequest Request;
	Request.Source = ESteamJoinSource::Overlay;
	Request.InviterId = Friend;

	if (Intent.Type == ESteamJoinIntentType::Server)
	{
		Request.ServerAddress = Intent.ServerAddress;
		ProcessRequest(Request);
	}
	else if (Intent.Type == ESteamJoinIntentType::Lobby)
	{
		Request.LobbyId = Intent.LobbyId;
		BeginLobbyLookup(Request);
	}
}

void USteamSessionsSubsystem::HandleFindByIdComplete(bool bSuccess, const FSteamSessionResult& Result)
{
	if (!LookupRequest.IsSet())
	{
		return;
	}

	FSteamJoinRequest Request = *LookupRequest;
	LookupRequest.Reset();

	if (!bSuccess)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: lobby %s could not be found."), *Request.LobbyId.ToString());
		OnSessionJoinFinished.Broadcast(FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound,
			NSLOCTEXT("SandwichSteam", "SessionsLobbyGone", "The lobby does not exist any more.")), FString());
		return;
	}

	Request.ResultHandle = Result.Handle;
	if (!Request.LobbyId.IsValid())
	{
		Request.LobbyId = Result.LobbyId;
	}
	ProcessRequest(Request);
}

void USteamSessionsSubsystem::BeginLobbyLookup(FSteamJoinRequest Request)
{
	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	if (!Backend.IsValid() || SandwichSteam::Sessions::IsDuplicateRequest(FPlatformTime::Seconds(), LastRequestTime, LastRequestLobby, Request.LobbyId, Settings ? Settings->DuplicateRequestSeconds : 0.0f))
	{
		++DuplicateCount;
		return;
	}

	LookupRequest = Request;
	if (!Backend->FindSessionById(Request.LobbyId))
	{
		LookupRequest.Reset();
		OnSessionJoinFinished.Broadcast(FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound,
			NSLOCTEXT("SandwichSteam", "SessionsLookupRefused", "Steam could not look up the lobby.")), FString());
	}
}

void USteamSessionsSubsystem::ProcessRequest(FSteamJoinRequest Request)
{
	if (!Backend.IsValid())
	{
		return;
	}

	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	const double Now = FPlatformTime::Seconds();
	if (SandwichSteam::Sessions::IsDuplicateRequest(Now, LastRequestTime, LastRequestLobby, Request.LobbyId, Settings ? Settings->DuplicateRequestSeconds : 0.0f))
	{
		++DuplicateCount;
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam sessions: dropped a second request for lobby %s."), *Request.LobbyId.ToString());
		return;
	}

	LastRequestLobby = Request.LobbyId;
	LastRequestTime = Now;
	++RequestCount;

	Request.bInMatch = Backend->IsInSession();
	const bool bAlreadyThere = Request.LobbyId.IsValid() && Backend->IsInLobby(Request.LobbyId);
	Request.Action = SandwichSteam::Sessions::DecideJoin(
		Settings ? Settings->JoinWhileInMatch : ESteamJoinInMatchPolicy::AskGame,
		Request.bInMatch,
		Settings ? Settings->bAutoJoinRequests : true,
		bAlreadyThere);

	// Something else is in flight: joining now would fail. Let the game know and drop it.
	if ((Request.Action == ESteamJoinAction::JoinNow || Request.Action == ESteamJoinAction::LeaveAndJoin) && Busy != EBusyOp::None)
	{
		Request.Action = ESteamJoinAction::Ignore;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: join request (%s) for %s, in match: %s, action: %s"),
		ToText(Request.Source), Request.LobbyId.IsValid() ? *Request.LobbyId.ToString() : *Request.ServerAddress, Request.bInMatch ? TEXT("yes") : TEXT("no"), ToText(Request.Action));

	PendingRequest.Reset();
	if (Request.Action == ESteamJoinAction::AskGame)
	{
		PendingRequest = Request;
	}

	OnJoinRequested.Broadcast(Request);

	if (Request.Action == ESteamJoinAction::JoinNow || Request.Action == ESteamJoinAction::LeaveAndJoin)
	{
		ExecuteRequest(Request);
	}
}

void USteamSessionsSubsystem::ExecuteRequest(const FSteamJoinRequest& Request)
{
	PendingRequest.Reset();

	const auto Fail = [this](const FSteamResult& Result)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: join request failed: %s"), *Result.Message.ToString());
		OnSessionJoinFinished.Broadcast(Result, FString());
	};

	if (!Request.ServerAddress.IsEmpty())
	{
		// Direct connect: no lobby. Leave the current session first when there is one.
		const auto Connect = [this, Address = Request.ServerAddress]()
		{
			const FSteamResult Travel = ClientTravelTo(Address);
			LastConnectString = Address;
			OnSessionJoinFinished.Broadcast(Travel, Address);
		};

		if (IsInSession())
		{
			const FSteamResult Left = DestroySession(FSteamSessionOpDelegate::CreateWeakLambda(this, [this, Connect, Fail](const FSteamResult& Result)
			{
				if (Result.IsSuccess())
				{
					Connect();
				}
				else
				{
					Fail(Result);
				}
			}));
			if (!Left.IsSuccess())
			{
				Fail(Left);
			}
		}
		else
		{
			Connect();
		}
		return;
	}

	if (IsInSession())
	{
		const int32 Handle = Request.ResultHandle;
		const FSteamResult Left = DestroySession(FSteamSessionOpDelegate::CreateWeakLambda(this, [this, Handle, Fail](const FSteamResult& Result)
		{
			if (!Result.IsSuccess())
			{
				Fail(Result);
				return;
			}

			const FSteamResult Started = StartJoin(Handle, FSteamSessionJoinDelegate());
			if (!Started.IsSuccess())
			{
				Fail(Started);
			}
		}));
		if (!Left.IsSuccess())
		{
			Fail(Left);
		}
		return;
	}

	const FSteamResult Started = StartJoin(Request.ResultHandle, FSteamSessionJoinDelegate());
	if (!Started.IsSuccess())
	{
		Fail(Started);
	}
}

void USteamSessionsSubsystem::ScheduleLaunchIntent()
{
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	if (!SteamCoreSubsystem || !SteamCoreSubsystem->HasPendingJoinIntent() || IsRunningDedicatedServer())
	{
		return;
	}

	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	LaunchIntentDeadline = FPlatformTime::Seconds() + (Settings ? Settings->LaunchJoinWaitSeconds : 30.0f);

	// Travelling needs a player controller, and the feature starts before the first map has one.
	LaunchIntentHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		const UGameInstance* Instance = GetGameInstance();
		if (!Instance || !Instance->GetFirstLocalPlayerController())
		{
			if (FPlatformTime::Seconds() > LaunchIntentDeadline)
			{
				LaunchIntentHandle.Reset();
				UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: the game was launched to join a lobby, but no local player controller appeared in time. The join is dropped."));
				return false;
			}
			return true;
		}

		LaunchIntentHandle.Reset();
		RouteLaunchIntent();
		return false;
	}), 0.5f);
}

void USteamSessionsSubsystem::RouteLaunchIntent()
{
	USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	FSteamJoinIntent Intent;
	if (!SteamCoreSubsystem || !SteamCoreSubsystem->ConsumePendingJoinIntent(Intent))
	{
		return;
	}

	FSteamJoinRequest Request;
	Request.Source = ESteamJoinSource::LaunchArg;
	if (Intent.Type == ESteamJoinIntentType::Server)
	{
		Request.ServerAddress = Intent.ServerAddress;
		ProcessRequest(Request);
	}
	else if (Intent.Type == ESteamJoinIntentType::Lobby)
	{
		Request.LobbyId = Intent.LobbyId;
		BeginLobbyLookup(Request);
	}
}

void USteamSessionsSubsystem::HandleSessionLost(const FGameplayTag& Reason, const FString& Message)
{
	if (!Backend.IsValid() || Busy == EBusyOp::Destroying)
	{
		return; // Already leaving.
	}

	UE_LOG(LogSandwichSteam, Warning, TEXT("Steam sessions: session lost (%s): %s"), *Reason.ToString(), *Message);
	OnSessionLost.Broadcast(Reason, Message);

	// The listener may have left already. Whatever is left is closed, so no lobby stays behind.
	if (Backend.IsValid() && Backend->IsInSession() && Busy != EBusyOp::Destroying)
	{
		Busy = EBusyOp::Destroying;
		if (!Backend->DestroySession())
		{
			Busy = EBusyOp::None;
		}
	}
}

void USteamSessionsSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* /*NetDriver*/, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || !IsInSession())
	{
		return;
	}

	// On a server a lost connection is one client leaving, not the end of the session. Only the driver itself failing is.
	const ENetMode Mode = World->GetNetMode();
	if (Mode == NM_ListenServer || Mode == NM_DedicatedServer)
	{
		const bool bDriverFailure = FailureType == ENetworkFailure::NetDriverAlreadyExists
			|| FailureType == ENetworkFailure::NetDriverCreateFailure
			|| FailureType == ENetworkFailure::NetDriverListenFailure;
		if (!bDriverFailure)
		{
			return;
		}
	}

	const ELostReason Reason = SandwichSteam::Sessions::ClassifyNetworkFailure(FailureType);
	HandleSessionLost(ToErrorTag(Reason), FString::Printf(TEXT("%s (%s)"), SandwichSteam::Sessions::LexToString(Reason), *ErrorString));
}

void USteamSessionsSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type /*FailureType*/, const FString& ErrorString)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || !IsInSession())
	{
		return;
	}

	HandleSessionLost(FGameplayTag(SteamGameplayTags::Error_Failed), FString::Printf(TEXT("Travel failed (%s)"), *ErrorString));
}

void USteamSessionsSubsystem::HandleGameModeInitialized(AGameModeBase* GameMode)
{
	if (!GameMode || !GameMode->GetWorld() || GameMode->GetWorld()->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// The game session (and its GameMode) is recreated on every travel, so Max Players is re-applied here too.
	ApplyMaxPlayersToGameSession(GameMode);
}

void USteamSessionsSubsystem::ApplyMaxPlayersToGameSession(AGameModeBase* GameModeOverride) const
{
	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	if (!Settings || !Settings->bApplyMaxPlayersToGameSession || !Backend.IsValid() || !Backend->IsHost() || HostedSettings.MaxPlayers <= 0)
	{
		return;
	}

	AGameModeBase* GameMode = GameModeOverride;
	if (!GameMode)
	{
		const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
		GameMode = World ? World->GetAuthGameMode() : nullptr;
	}

	if (GameMode && GameMode->GameSession)
	{
		GameMode->GameSession->MaxPlayers = HostedSettings.MaxPlayers;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam sessions: set GameSession::MaxPlayers to %d"), HostedSettings.MaxPlayers);
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamSessionsSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Sessions: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	Report += FString::Printf(TEXT("  Definition: %s (%d session profiles)\n"), Definition ? *Definition->GetName() : TEXT("none"), Definition ? Definition->Sessions.Num() : 0);

	if (Definition)
	{
		for (const FSteamSessionProfileDef& Profile : Definition->Sessions)
		{
			Report += FString::Printf(TEXT("    [%s] %d players, %s, presence %s, join in progress %s, %d setting(s)\n"),
				*Profile.Tag.ToString(), Profile.MaxPlayers,
				Profile.Visibility == ESteamSessionVisibility::Public ? TEXT("public") : (Profile.Visibility == ESteamSessionVisibility::FriendsOnly ? TEXT("friends only") : TEXT("private")),
				Profile.bUsesPresence ? TEXT("yes") : TEXT("no"), Profile.bAllowJoinInProgress ? TEXT("yes") : TEXT("no"), Profile.Settings.Num());
		}
	}

	const TCHAR* BusyText = TEXT("idle");
	switch (Busy)
	{
	case EBusyOp::Creating: BusyText = TEXT("creating"); break;
	case EBusyOp::Updating: BusyText = TEXT("updating"); break;
	case EBusyOp::Joining: BusyText = TEXT("joining"); break;
	case EBusyOp::Destroying: BusyText = TEXT("destroying"); break;
	case EBusyOp::CreatingLobby: BusyText = TEXT("creating lobby"); break;
	case EBusyOp::JoiningLobby: BusyText = TEXT("joining lobby"); break;
	default: break;
	}
	Report += FString::Printf(TEXT("  Backend: %s, local user %s, operation: %s, searching: %s\n"),
		Backend.IsValid() ? TEXT("ready") : TEXT("none"),
		Backend.IsValid() ? *Backend->GetLocalSteamId().ToString() : TEXT("-"), BusyText, bFinding ? TEXT("yes") : TEXT("no"));

	if (Backend.IsValid())
	{
		Report += FString::Printf(TEXT("  Hosted profile: %s, lobby: %s, cached search results: %d\n"), ActiveProfile.IsValid() ? *ActiveProfile.ToString() : TEXT("-"),
			Backend->GetCurrentLobbyId().IsValid() ? *Backend->GetCurrentLobbyId().ToString() : TEXT("-"), Backend->GetCachedResultCount());
		Report += TEXT("  Session: ") + Backend->DescribeSession().Replace(TEXT("\n"), TEXT("\n    ")) + TEXT("\n");

		if (IsInSession())
		{
			FSteamSessionSettings CurrentSettings;
			FGameplayTag CurrentProfile;
			GetCurrentSettings(CurrentSettings, CurrentProfile);
			Report += FString::Printf(TEXT("  Applied settings: name '%s', %d players, %s, join in progress %s, presence %s, %d setting(s), profile %s\n"),
				*CurrentSettings.DisplayName, CurrentSettings.MaxPlayers,
				CurrentSettings.Visibility == ESteamSessionVisibility::Public ? TEXT("public") : (CurrentSettings.Visibility == ESteamSessionVisibility::FriendsOnly ? TEXT("friends only") : TEXT("private")),
				CurrentSettings.bAllowJoinInProgress ? TEXT("yes") : TEXT("no"), CurrentSettings.bUsesPresence ? TEXT("yes") : TEXT("no"), CurrentSettings.Settings.Num(),
				CurrentProfile.IsValid() ? *CurrentProfile.ToString() : TEXT("none"));

			const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
			const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
			Report += FString::Printf(TEXT("  Game session max players: %s\n"), (GameMode && GameMode->GameSession) ? *FString::FromInt(GameMode->GameSession->MaxPlayers) : TEXT("- (not the host, or no game mode yet)"));
		}
	}

	const FSteamId ActiveLobby = GetActiveLobbyId();
	Report += FString::Printf(TEXT("  Lobby: %s (%s), searching lobbies: %s, chat messages received: %d\n"),
		ActiveLobby.IsValid() ? *ActiveLobby.ToString() : TEXT("-"),
		LobbyOnlyId.IsValid() ? TEXT("lobby only") : (ActiveLobby.IsValid() ? TEXT("session lobby") : TEXT("none")),
		bFindingLobbies ? TEXT("yes") : TEXT("no"), ChatReceivedCount);
	if (ActiveLobby.IsValid() && LobbyBackend.IsValid())
	{
		const TArray<FSteamLobbyMember> Members = GetLobbyMembers();
		Report += FString::Printf(TEXT("    Owner: %s%s, members: %d / %d, all ready: %s\n"), *GetLobbyOwnerId().ToString(), IsLobbyOwner() ? TEXT(" (local)") : TEXT(""),
			Members.Num(), GetLobbyMemberLimit(), AreAllMembersReady() ? TEXT("yes") : TEXT("no"));
		for (const FSteamLobbyMember& Member : Members)
		{
			Report += FString::Printf(TEXT("    - %s %s%s%s, ready: %s\n"), *Member.Id.ToString(), *Member.Name, Member.bIsOwner ? TEXT(" [owner]") : TEXT(""), Member.bIsLocal ? TEXT(" [you]") : TEXT(""), Member.bReady ? TEXT("yes") : TEXT("no"));
		}

		for (const TPair<FString, FString>& Pair : GetAllLobbyData())
		{
			Report += FString::Printf(TEXT("    data %s = %s\n"), *Pair.Key, *Pair.Value.Left(80));
		}
	}

	if (PendingRequest.IsSet())
	{
		Report += FString::Printf(TEXT("  Waiting join request (%s) for %s: call Accept or Decline\n"), ToText(PendingRequest->Source),
			PendingRequest->LobbyId.IsValid() ? *PendingRequest->LobbyId.ToString() : *PendingRequest->ServerAddress);
	}

	const USteamSessionsSettings* Settings = USteamSessionsSettings::Get();
	Report += FString::Printf(TEXT("  Join requests: %d handled, %d duplicates dropped. Auto join: %s, auto travel: %s\n"), RequestCount, DuplicateCount,
		(Settings && Settings->bAutoJoinRequests) ? TEXT("yes") : TEXT("no"), (Settings && Settings->bAutoTravelAfterJoin) ? TEXT("yes") : TEXT("no"));
	Report += FString::Printf(TEXT("  Last connect string: %s"), LastConnectString.IsEmpty() ? TEXT("-") : *LastConnectString);
	return Report;
}
#endif // SANDWICHSTEAM_WITH_DEBUG
