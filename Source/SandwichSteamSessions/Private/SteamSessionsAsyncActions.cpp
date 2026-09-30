// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamSessionsAsyncActions.h"
#include "SteamSessionsSubsystem.h"

USteamCreateSessionAsyncAction* USteamCreateSessionAsyncAction::CreateSteamSession(const UObject* WorldContextObject, FGameplayTag ProfileTag)
{
	USteamCreateSessionAsyncAction* Action = NewObject<USteamCreateSessionAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->ProfileTag = ProfileTag;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamCreateSessionAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamCreateSessionAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->CreateSession(ProfileTag,
		FSteamSessionOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result)
		{
			if (Result.IsSuccess())
			{
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

USteamCreateSessionWithSettingsAsyncAction* USteamCreateSessionWithSettingsAsyncAction::CreateSteamSessionWithSettings(const UObject* WorldContextObject, const FSteamSessionSettings& InSettings)
{
	USteamCreateSessionWithSettingsAsyncAction* Action = NewObject<USteamCreateSessionWithSettingsAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Settings = InSettings;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamCreateSessionWithSettingsAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamCreateSessionWithSettingsAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->CreateSession(Settings,
		FSteamSessionSettingsOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const FSteamSessionSettings& InApplied)
		{
			if (Result.IsSuccess())
			{
				Applied = InApplied;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamCreateSessionWithSettingsAsyncAction::BroadcastSuccess()
{
	OnCreated.Broadcast(Applied);
	Super::BroadcastSuccess();
}

USteamCreateSessionFromProfileAsyncAction* USteamCreateSessionFromProfileAsyncAction::CreateSteamSessionFromProfile(const UObject* WorldContextObject, FGameplayTag InProfileTag, const FSteamSessionSettings& InSettings)
{
	USteamCreateSessionFromProfileAsyncAction* Action = NewObject<USteamCreateSessionFromProfileAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->ProfileTag = InProfileTag;
	Action->Settings = InSettings;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamCreateSessionFromProfileAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamCreateSessionFromProfileAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->CreateSession(ProfileTag, Settings,
		FSteamSessionSettingsOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const FSteamSessionSettings& InApplied)
		{
			if (Result.IsSuccess())
			{
				Applied = InApplied;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamCreateSessionFromProfileAsyncAction::BroadcastSuccess()
{
	OnCreated.Broadcast(Applied);
	Super::BroadcastSuccess();
}

USteamUpdateSessionAsyncAction* USteamUpdateSessionAsyncAction::UpdateSteamSession(const UObject* WorldContextObject, const FSteamSessionSettings& InSettings)
{
	USteamUpdateSessionAsyncAction* Action = NewObject<USteamUpdateSessionAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Settings = InSettings;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamUpdateSessionAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamUpdateSessionAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->UpdateSession(Settings,
		FSteamSessionSettingsOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const FSteamSessionSettings& InApplied)
		{
			if (Result.IsSuccess())
			{
				Applied = InApplied;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamUpdateSessionAsyncAction::BroadcastSuccess()
{
	OnUpdated.Broadcast(Applied);
	Super::BroadcastSuccess();
}

USteamFindSessionsAsyncAction* USteamFindSessionsAsyncAction::FindSteamSessions(const UObject* WorldContextObject, const FSteamSessionSearchOptions& Options)
{
	USteamFindSessionsAsyncAction* Action = NewObject<USteamFindSessionsAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Options = Options;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamFindSessionsAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamFindSessionsAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->FindSessions(Options,
		FSteamSessionFindDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const TArray<FSteamSessionResult>& Results)
		{
			if (Result.IsSuccess())
			{
				Found = Results;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamFindSessionsAsyncAction::BroadcastSuccess()
{
	OnSessionsFound.Broadcast(Found);
	Super::BroadcastSuccess();
}

USteamJoinSessionAsyncAction* USteamJoinSessionAsyncAction::JoinSteamSession(const UObject* WorldContextObject, const FSteamSessionResult& Session)
{
	USteamJoinSessionAsyncAction* Action = NewObject<USteamJoinSessionAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Session = Session;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamJoinSessionAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamJoinSessionAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->JoinSession(Session,
		FSteamSessionJoinDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const FString& Connect)
		{
			if (Result.IsSuccess())
			{
				ConnectString = Connect;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamJoinSessionAsyncAction::BroadcastSuccess()
{
	OnJoined.Broadcast(ConnectString);
	Super::BroadcastSuccess();
}

USteamDestroySessionAsyncAction* USteamDestroySessionAsyncAction::DestroySteamSession(const UObject* WorldContextObject)
{
	USteamDestroySessionAsyncAction* Action = NewObject<USteamDestroySessionAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamDestroySessionAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamDestroySessionAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->DestroySession(
		FSteamSessionOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result)
		{
			if (Result.IsSuccess())
			{
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

USteamCreateLobbyAsyncAction* USteamCreateLobbyAsyncAction::CreateSteamLobby(const UObject* WorldContextObject, FGameplayTag ProfileTag)
{
	USteamCreateLobbyAsyncAction* Action = NewObject<USteamCreateLobbyAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->ProfileTag = ProfileTag;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamCreateLobbyAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamCreateLobbyAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->CreateLobby(ProfileTag,
		FSteamLobbyOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, FSteamId Lobby)
		{
			if (Result.IsSuccess())
			{
				LobbyId = Lobby;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamCreateLobbyAsyncAction::BroadcastSuccess()
{
	OnLobbyEntered.Broadcast(LobbyId);
	Super::BroadcastSuccess();
}

USteamCreateLobbyWithSettingsAsyncAction* USteamCreateLobbyWithSettingsAsyncAction::CreateSteamLobbyWithSettings(const UObject* WorldContextObject, const FSteamSessionSettings& InSettings)
{
	USteamCreateLobbyWithSettingsAsyncAction* Action = NewObject<USteamCreateLobbyWithSettingsAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Settings = InSettings;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamCreateLobbyWithSettingsAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamCreateLobbyWithSettingsAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->CreateLobby(Settings,
		FSteamLobbySettingsOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, FSteamId Lobby, const FSteamSessionSettings& InApplied)
		{
			if (Result.IsSuccess())
			{
				LobbyId = Lobby;
				Applied = InApplied;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamCreateLobbyWithSettingsAsyncAction::BroadcastSuccess()
{
	OnLobbyEntered.Broadcast(LobbyId, Applied);
	Super::BroadcastSuccess();
}

USteamCreateLobbyFromProfileAsyncAction* USteamCreateLobbyFromProfileAsyncAction::CreateSteamLobbyFromProfile(const UObject* WorldContextObject, FGameplayTag InProfileTag, const FSteamSessionSettings& InSettings)
{
	USteamCreateLobbyFromProfileAsyncAction* Action = NewObject<USteamCreateLobbyFromProfileAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->ProfileTag = InProfileTag;
	Action->Settings = InSettings;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamCreateLobbyFromProfileAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamCreateLobbyFromProfileAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->CreateLobby(ProfileTag, Settings,
		FSteamLobbySettingsOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, FSteamId Lobby, const FSteamSessionSettings& InApplied)
		{
			if (Result.IsSuccess())
			{
				LobbyId = Lobby;
				Applied = InApplied;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamCreateLobbyFromProfileAsyncAction::BroadcastSuccess()
{
	OnLobbyEntered.Broadcast(LobbyId, Applied);
	Super::BroadcastSuccess();
}

USteamJoinLobbyAsyncAction* USteamJoinLobbyAsyncAction::JoinSteamLobby(const UObject* WorldContextObject, FSteamId LobbyId)
{
	USteamJoinLobbyAsyncAction* Action = NewObject<USteamJoinLobbyAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->LobbyId = LobbyId;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamJoinLobbyAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamJoinLobbyAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->JoinLobby(LobbyId,
		FSteamLobbyOpDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, FSteamId Lobby)
		{
			if (Result.IsSuccess())
			{
				EnteredId = Lobby;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamJoinLobbyAsyncAction::BroadcastSuccess()
{
	OnLobbyEntered.Broadcast(EnteredId);
	Super::BroadcastSuccess();
}

USteamFindLobbiesAsyncAction* USteamFindLobbiesAsyncAction::FindSteamLobbies(const UObject* WorldContextObject, const FSteamSessionSearchOptions& Options)
{
	USteamFindLobbiesAsyncAction* Action = NewObject<USteamFindLobbiesAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->Options = Options;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamFindLobbiesAsyncAction::GetFeatureClass() const
{
	return USteamSessionsSubsystem::StaticClass();
}

void USteamFindLobbiesAsyncAction::StartRequest()
{
	USteamSessionsSubsystem* Sessions = Cast<USteamSessionsSubsystem>(GetFeature());
	if (!Sessions)
	{
		return;
	}

	const FSteamResult Started = Sessions->FindLobbies(Options,
		FSteamLobbyFindDelegate::CreateWeakLambda(this, [this](const FSteamResult& Result, const TArray<FSteamLobbyInfo>& Lobbies)
		{
			if (Result.IsSuccess())
			{
				Found = Lobbies;
				FinishSuccess();
			}
			else
			{
				FinishFailure(Result);
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamFindLobbiesAsyncAction::BroadcastSuccess()
{
	OnLobbiesFound.Broadcast(Found);
	Super::BroadcastSuccess();
}
