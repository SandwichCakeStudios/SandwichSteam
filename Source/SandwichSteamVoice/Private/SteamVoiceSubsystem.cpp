// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceSubsystem.h"
#include "Core/SteamBackend.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Core/SteamSDK.h"
#include "SteamVoiceOssFallback.h"
#include "SteamVoiceComponent.h"
#include "SteamVoiceRouting.h"
#include "SteamVoiceRules.h"
#include "SteamVoiceSettings.h"
#include "SteamVoiceTransport.h"
#include "SteamVoiceWorker.h"

using SandwichSteam::Voice::FMuteBook;
using SandwichSteam::Voice::FTalkingChange;
using SandwichSteam::Voice::FTalkingTracker;

namespace
{
	/** Steam has one microphone and one message channel per process: only one game instance (PIE) runs the pipeline. Game thread only. */
	TWeakObjectPtr<USteamVoiceSubsystem> GPipelineOwner;

	/** The signed in Steam user (raw). Invalid when Steam is not ready. */
	FSteamId GetSignedInSteamId(const UGameInstance* GameInstance)
	{
#if SANDWICHSTEAM_WITH_STEAMWORKS
		if (SandwichSteam::IsSteamClientReady(GameInstance))
		{
			if (ISteamUser* User = SteamUser())
			{
				return FSteamId(static_cast<int64>(User->GetSteamID().ConvertToUint64()));
			}
		}
#endif
		return FSteamId();
	}

	/** True when the local user has blocked the player on Steam. False when unknown (Steam not ready). */
	bool IsBlockedOnSteam(const UGameInstance* GameInstance, FSteamId Player)
	{
#if SANDWICHSTEAM_WITH_STEAMWORKS
		if (Player.IsValid() && SandwichSteam::IsSteamClientReady(GameInstance))
		{
			if (ISteamFriends* Friends = SteamFriends())
			{
				return Friends->GetFriendRelationship(CSteamID(static_cast<uint64>(Player.Value))) == k_EFriendRelationshipBlocked;
			}
		}
#endif
		return false;
	}
}

USteamVoiceSubsystem* USteamVoiceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamVoiceSubsystem>() : nullptr;
}

USteamVoiceSubsystem::USteamVoiceSubsystem() = default;
USteamVoiceSubsystem::~USteamVoiceSubsystem() = default;

FGameplayTag USteamVoiceSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Voice;
}

bool USteamVoiceSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	OssFallback = MakeShared<FSteamVoiceOssFallback>(GetGameInstance());

	// The Online Subsystem voice and the Steam pipeline would both read Steam's microphone: only one of them runs.
	bUseSteamPipeline = !OssFallback->IsUsable();
	if (bUseSteamPipeline)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: using the Steam voice pipeline (the Online Subsystem voice interface is off)."));
	}
	else
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: using the Online Subsystem voice ([OnlineSubsystem] bHasVoiceEnabled=true). Remove bHasVoiceEnabled and [Voice] bEnabled from DefaultEngine.ini to use the Steam voice pipeline."));
	}
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &USteamVoiceSubsystem::HandleWorldCleanup);

	Tracker = MakeUnique<FTalkingTracker>();
	Book = MakeUnique<FMuteBook>();

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	bEnabled = Settings ? Settings->bEnabledByDefault : true;
	Mode = Settings ? Settings->DefaultMode : ESteamVoiceMode::PushToTalk;
	MasterVolume = Settings ? Settings->DefaultMasterVolume : 1.0f;

	LocalId = GetSignedInSteamId(GetGameInstance());
	bTalkRequested = false;
	bMicOpen = false;
	bPushToTalkUnavailable = false;
	LastGameState.Reset();
	Present.Reset();
	Evaluated.Reset();
	TalkingEventsSent = 0;
	MuteCallsFailed = 0;

	if (bEnabled)
	{
		ApplyEnabledState();
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: %s, %s."), bEnabled ? TEXT("enabled") : TEXT("disabled (Enabled By Default is off)"), Mode == ESteamVoiceMode::PushToTalk ? TEXT("push to talk") : TEXT("open mic"));
	return true;
#else
	return false;
#endif
}

void USteamVoiceSubsystem::ShutdownFeature()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	UnbindPushToTalk();

	if (Tracker.IsValid())
	{
		TArray<FTalkingChange> Stopped;
		Tracker->Clear(Stopped);
		for (const FTalkingChange& Change : Stopped)
		{
			BroadcastTalking(Change.Player, false);
		}
	}

	StopPipeline();
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}

	if (OssFallback.IsValid() && bMicOpen && !bUseSteamPipeline)
	{
		OssFallback->StopTalking();
	}
	bMicOpen = false;
	bTalkRequested = false;
	bEnabled = false;

	RemoveVolumeMix();

	Present.Reset();
	Evaluated.Reset();
	LastGameState.Reset();
	Book.Reset();
	Tracker.Reset();
	OssFallback.Reset();
}

FSteamResult USteamVoiceSubsystem::SetVoiceEnabled(bool bNewEnabled)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (bNewEnabled != bEnabled)
	{
		bEnabled = bNewEnabled;
		ApplyEnabledState();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: %s."), bEnabled ? TEXT("enabled") : TEXT("disabled"));
	}
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::SetVoiceMode(ESteamVoiceMode NewMode)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (NewMode != Mode)
	{
		Mode = NewMode;
		RefreshTalking(false);
		OnVoiceModeChanged.Broadcast(Mode);
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: mode is now %s."), Mode == ESteamVoiceMode::PushToTalk ? TEXT("push to talk") : TEXT("open mic"));
	}
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::StartTalking()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!bEnabled)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "VoiceDisabled", "Voice is disabled. Call Set Voice Enabled first."));
	}

	bTalkRequested = true;
	RefreshTalking(false);
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::StopTalking()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	bTalkRequested = false;
	RefreshTalking(false);
	return FSteamResult::Success();
}

bool USteamVoiceSubsystem::IsLocalTalking() const
{
	return Tracker.IsValid() && LocalId.IsValid() && Tracker->IsTalking(LocalId.Value);
}

bool USteamVoiceSubsystem::IsHeadsetPresent() const
{
	if (bUseSteamPipeline)
	{
#if SANDWICHSTEAM_WITH_STEAMWORKS
		// Steam has no headset query: the capture says NotInitialized when there is no recording device.
		return Worker.IsValid() && Worker->GetStats().LastCaptureResult != static_cast<int32>(k_EVoiceResultNotInitialized);
#else
		return false;
#endif
	}
	return OssFallback.IsValid() && OssFallback->IsHeadsetPresent();
}

TArray<FSteamId> USteamVoiceSubsystem::GetPlayersInGame() const
{
	TArray<FSteamId> Players;
	Players.Reserve(Present.Num());
	for (const int64 Player : Present)
	{
		Players.Add(FSteamId(Player));
	}
	return Players;
}

bool USteamVoiceSubsystem::IsPlayerTalking(FSteamId Player) const
{
	return Tracker.IsValid() && Tracker->IsTalking(Player.Value);
}

TArray<FSteamId> USteamVoiceSubsystem::GetTalkingPlayers() const
{
	TArray<FSteamId> Players;
	if (Tracker.IsValid())
	{
		for (const int64 Player : Tracker->GetTalking())
		{
			Players.Add(FSteamId(Player));
		}
	}
	return Players;
}

FSteamResult USteamVoiceSubsystem::MutePlayer(FSteamId Player, bool bSystemWide)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Player.IsValid() || Player == LocalId)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceMuteInvalid", "Pass the Steam ID of another player. You cannot mute yourself."));
	}

	if (Book->SetUserMuted(Player.Value, true, bSystemWide))
	{
		OnPlayerMuteChanged.Broadcast(Player, true, false);
	}

	PublishRouting();
	if (Present.Contains(Player.Value))
	{
		ApplyMute(Player, Player.ToUniqueNetId(GetGameInstance()));
	}
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::UnmutePlayer(FSteamId Player)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Player.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceUnmuteInvalid", "Pass the Steam ID of the player to unmute."));
	}

	if (Book->SetUserMuted(Player.Value, false, false))
	{
		OnPlayerMuteChanged.Broadcast(Player, false, false);
	}

	PublishRouting();
	if (Present.Contains(Player.Value))
	{
		ApplyMute(Player, Player.ToUniqueNetId(GetGameInstance()));
	}
	return FSteamResult::Success();
}

bool USteamVoiceSubsystem::IsPlayerMuted(FSteamId Player) const
{
	return Book.IsValid() && Book->IsMuted(Player.Value);
}

TArray<FSteamId> USteamVoiceSubsystem::GetMutedPlayers() const
{
	TArray<FSteamId> Players;
	if (Book.IsValid())
	{
		for (const int64 Player : Book->GetMutedPlayers())
		{
			Players.Add(FSteamId(Player));
		}
	}
	return Players;
}

FSteamResult USteamVoiceSubsystem::SetMasterVolume(float Volume)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	MasterVolume = FMath::Clamp(Volume, 0.0f, 2.0f);
	if (!bEnabled)
	{
		return FSteamResult::Success(); // Applied when voice is enabled.
	}

	if (!ApplyMasterVolume())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
			NSLOCTEXT("SandwichSteam", "VoiceNoSoundClass", "Assign a Voice Sound Class and a Voice Sound Mix in Project Settings > Plugins > Sandwich Steam - Voice, and route the voice output to that sound class."));
	}
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::BindPushToTalk(APlayerController* Controller)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	if (!Settings || Settings->PushToTalkAction.IsNull())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceNoAction", "Set a Push To Talk Action in Project Settings > Plugins > Sandwich Steam - Voice."));
	}

	if (!Controller || !Cast<UEnhancedInputComponent>(Controller->InputComponent))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceNoInputComponent", "The controller has no Enhanced Input component yet (call this after the controller's input was set up, and check Project Settings > Engine > Input > Default Input Component Class)."));
	}

	bPushToTalkUnavailable = false;
	if (!TryBindPushToTalk(Controller))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "VoiceBindFailed", "The push to talk action could not be loaded."));
	}
	return FSteamResult::Success();
}

bool USteamVoiceSubsystem::IsPushToTalkBound() const
{
	return BoundComponent.IsValid();
}

// ---- Runtime state ----

void USteamVoiceSubsystem::ApplyEnabledState()
{
	if (bEnabled)
	{
		if (bUseSteamPipeline)
		{
			StartPipeline();
		}

		if (!TickHandle.IsValid())
		{
			// The pipeline ticks every frame to start speakers on time; the player poll itself still runs at the poll rate.
			const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
			const float Interval = bUseSteamPipeline ? 0.0f : (Settings ? FMath::Clamp(Settings->TalkingPollSeconds, 0.02f, 1.0f) : 0.1f);
			PollAccumulator = TNumericLimits<float>::Max(); // Poll on the first tick.
			TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USteamVoiceSubsystem::TickVoice), Interval);
		}

		bPushToTalkUnavailable = false;
		LastGameState.Reset(); // The next poll treats the current game as a new session: mutes are applied again.
		ApplyMasterVolume();
		RefreshTalking(true);
		return;
	}

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	bTalkRequested = false;
	UnbindPushToTalk();
	RefreshTalking(true);
	RemoveVolumeMix();
	StopPipeline();

	if (Tracker.IsValid())
	{
		TArray<FTalkingChange> Stopped;
		Tracker->Clear(Stopped);
		for (const FTalkingChange& Change : Stopped)
		{
			BroadcastTalking(Change.Player, false);
		}
	}
	Present.Reset();
	Evaluated.Reset();
}

bool USteamVoiceSubsystem::ShouldTalk() const
{
	return bEnabled && (Mode == ESteamVoiceMode::OpenMic || bTalkRequested);
}

void USteamVoiceSubsystem::RefreshTalking(bool bForce)
{
	if (!OssFallback.IsValid())
	{
		return;
	}

	const bool bWant = ShouldTalk();
	if (bWant == bMicOpen && !bForce)
	{
		return;
	}

	if (bUseSteamPipeline)
	{
		bMicOpen = bWant;
		PublishRouting(); // The worker starts or stops Steam's recording.
		return;
	}

	bMicOpen = bWant;
	if (bWant)
	{
		OssFallback->StartTalking();
	}
	else
	{
		OssFallback->StopTalking();
	}
}

void USteamVoiceSubsystem::HandlePushToTalkPressed()
{
	bTalkRequested = true;
	RefreshTalking(false);
}

void USteamVoiceSubsystem::HandlePushToTalkReleased()
{
	bTalkRequested = false;
	RefreshTalking(false);
}

bool USteamVoiceSubsystem::TickVoice(float DeltaTime)
{
	if (!OssFallback.IsValid() || !bEnabled)
	{
		TickHandle.Reset();
		return false; // Removes the ticker.
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();

	if (bUseSteamPipeline)
	{
		DrainWorkerEvents();

		// A new world: connect speakers for the talkers the worker knows, and play the ones that are talking.
		if (Worker.IsValid() && World && !World->bIsTearingDown && SpeakerWorld.Get() != World)
		{
			for (const TPair<int64, FSpeakerSource>& Source : SpeakerSources)
			{
				USteamVoiceComponent* Speaker = EnsureSpeaker(Source.Key);
				if (Speaker && Source.Value.bActive && !Speaker->IsPlaying())
				{
					Speaker->Start();
				}
			}
		}

		PollAccumulator += DeltaTime;
		const float Interval = Settings ? FMath::Clamp(Settings->TalkingPollSeconds, 0.02f, 1.0f) : 0.1f;
		if (PollAccumulator < Interval)
		{
			return true;
		}
		PollAccumulator = 0.0f;
	}

	if (!LocalId.IsValid())
	{
		LocalId = GetSignedInSteamId(GetGameInstance());
	}

	// A new game state means a new session (or a new map): talkers register again, so what the Online Subsystem knew is void.
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState != LastGameState.Get())
	{
		LastGameState = GameState;
		Evaluated.Reset();
		Book->ForgetApplied();
		if (GameState)
		{
			ApplyMasterVolume();
			if (bMicOpen && !bUseSteamPipeline)
			{
				OssFallback->StartTalking(); // The connection is new; open the microphone on it.
			}
		}
	}

	// Push to talk binding: the controller and its input component appear some time after the map loaded.
	if (Settings && Settings->bAutoBindPushToTalk && !bPushToTalkUnavailable && !Settings->PushToTalkAction.IsNull() && !IsPushToTalkBound())
	{
		if (APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr)
		{
			TryBindPushToTalk(Controller);
		}
	}

	TSet<int64> NowPresent;
	TSet<int64> NowTalking;
	TArray<TPair<FSteamId, TSharedPtr<const FUniqueNetId>>> Others;

	if (LocalId.IsValid())
	{
		NowPresent.Add(LocalId.Value);
		if (bUseSteamPipeline ? PipelineTalking.Contains(LocalId.Value) : OssFallback->IsLocalTalking())
		{
			NowTalking.Add(LocalId.Value);
		}
	}

	SeenPlayerStates = 0;
	SeenWithoutNetId = 0;
	SeenNotSteamId = 0;
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (!PlayerState)
			{
				continue;
			}

			++SeenPlayerStates;
			const TSharedPtr<const FUniqueNetId> NetId = PlayerState->GetUniqueId().GetUniqueNetId();
			FSteamId Id;
			if (!NetId.IsValid())
			{
				++SeenWithoutNetId;
				continue;
			}
			if (!FSteamId::FromUniqueNetId(*NetId, Id))
			{
				++SeenNotSteamId;
				NotSteamSample = FString::Printf(TEXT("type %s, id \"%s\""), *NetId->GetType().ToString(), *NetId->ToString());
				continue;
			}
			if (Id == LocalId)
			{
				continue;
			}

			NowPresent.Add(Id.Value);
			Others.Emplace(Id, NetId);
			if (bUseSteamPipeline ? PipelineTalking.Contains(Id.Value) : OssFallback->IsRemoteTalking(*NetId))
			{
				NowTalking.Add(Id.Value);
			}
		}
	}

	// Fake talkers (Steam.Voice.FakeTalker) count as present, so talking events and the test panel show them.
	for (const TPair<int64, FSpeakerSource>& Source : SpeakerSources)
	{
		if (Source.Value.bFake)
		{
			NowPresent.Add(Source.Key);
			if (PipelineTalking.Contains(Source.Key))
			{
				NowTalking.Add(Source.Key);
			}
		}
	}
	Present = MoveTemp(NowPresent);

	for (const TPair<FSteamId, TSharedPtr<const FUniqueNetId>>& Other : Others)
	{
		// Players blocked on Steam are muted once, when they first show up in this session.
		if (!Evaluated.Contains(Other.Key.Value))
		{
			Evaluated.Add(Other.Key.Value);
			if (Settings && Settings->bAutoMuteBlockedPlayers && IsBlockedOnSteam(GetGameInstance(), Other.Key) && Book->SetAutoMuted(Other.Key.Value, true))
			{
				OnPlayerMuteChanged.Broadcast(Other.Key, true, true);
			}
		}

		ApplyMute(Other.Key, Other.Value);
	}

	TArray<FTalkingChange> Changes;
	Tracker->Update(NowTalking, Present, Changes);
	for (const FTalkingChange& Change : Changes)
	{
		BroadcastTalking(Change.Player, Change.bTalking);
	}

	PublishRouting();
	return true;
}

void USteamVoiceSubsystem::BroadcastTalking(int64 Player, bool bTalking)
{
	++TalkingEventsSent;
	UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam voice: %s %s talking."), *FSteamId(Player).ToString(), bTalking ? TEXT("started") : TEXT("stopped"));
	OnPlayerTalkingChanged.Broadcast(FSteamId(Player), bTalking, Player == LocalId.Value);
}

void USteamVoiceSubsystem::ApplyMute(FSteamId Player, const TSharedPtr<const FUniqueNetId>& NetId)
{
	// The Steam pipeline reads mutes from the routing snapshot.
	if (bUseSteamPipeline || !OssFallback.IsValid() || !NetId.IsValid() || !Book->NeedsApply(Player.Value))
	{
		return;
	}

	if (OssFallback->SetMuted(*NetId, Book->IsMuted(Player.Value), Book->IsSystemWide(Player.Value)))
	{
		Book->MarkApplied(Player.Value);
	}
	else
	{
		++MuteCallsFailed; // The talker is probably not registered yet; the next poll tries again.
	}
}

// ---- Steam voice pipeline ----

bool USteamVoiceSubsystem::StartPipeline()
{
	if (Worker.IsValid())
	{
		return true;
	}

	if (GPipelineOwner.IsValid() && GPipelineOwner.Get() != this)
	{
		if (!bWarnedPipelineOwner)
		{
			bWarnedPipelineOwner = true;
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: another game instance in this process already runs Steam voice (Steam has one microphone per process). Voice stays off here; use Steam.Voice.FakeTalker in the first instance to test on one machine."));
		}
		return false;
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	SandwichSteam::Voice::FWorkerConfig Config;
	if (Settings)
	{
		Config.MessageChannel = FMath::Clamp(Settings->SteamMessageChannel, 0, 255);
		Config.RequestedSampleRate = Settings->DecodeSampleRate;
		Config.PacketsPerSecond = static_cast<float>(Settings->MaxPacketsPerSecond);
		Config.BytesPerSecond = static_cast<float>(Settings->MaxBytesPerSecond);
	}

	TSharedRef<FSteamVoiceTransport> NewTransport = MakeShared<FSteamVoiceTransport>(Config.MessageChannel);
	if (!NewTransport->IsAvailable())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: SteamNetworkingMessages is not available, the Steam voice pipeline cannot start."));
		return false;
	}

	TSharedRef<FSteamVoiceWorker> NewWorker = MakeShared<FSteamVoiceWorker>(Config, NewTransport);
	if (!NewWorker->StartThread())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: the voice worker thread could not be created."));
		return false;
	}

	Transport = NewTransport;
	Worker = NewWorker;
	GPipelineOwner = this;
	bRoutingPublished = false;
	PublishRouting();

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: pipeline started (message channel %d, decode %d Hz, Steam optimal %d Hz)."),
		Config.MessageChannel, Worker->GetDecodeSampleRate(), Worker->GetStats().OptimalSampleRate);
	return true;
}

void USteamVoiceSubsystem::StopPipeline()
{
	if (Worker.IsValid())
	{
		Worker->Shutdown(); // Joins the thread before the transport goes away.
		Worker.Reset();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: pipeline stopped."));
	}
	Transport.Reset();

	if (GPipelineOwner.Get() == this)
	{
		GPipelineOwner.Reset();
	}

	DestroySpeakers();
	SpeakerSources.Reset();
	PipelineTalking.Reset();
	bRoutingPublished = false;
}

void USteamVoiceSubsystem::PublishRouting()
{
	if (!Worker.IsValid())
	{
		return;
	}

	TArray<uint64> Peers;
	Peers.Reserve(Present.Num());
	for (const int64 Player : Present)
	{
		if (Player != LocalId.Value && !SandwichSteam::Voice::IsFakeTalkerId(static_cast<uint64>(Player)))
		{
			Peers.Add(static_cast<uint64>(Player));
		}
	}
	Peers.Sort();

	TArray<uint64> Muted;
	TArray<uint64> Blocked;
	if (Book.IsValid())
	{
		for (const int64 Player : Book->GetMutedPlayers())
		{
			if (Book->IsUserMuted(Player))
			{
				Muted.Add(static_cast<uint64>(Player));
			}
			if (Book->IsAutoMuted(Player))
			{
				Blocked.Add(static_cast<uint64>(Player));
			}
		}
	}
	Muted.Sort();
	Blocked.Sort();

	if (bRoutingPublished && bPublishedRecord == bMicOpen && PublishedLocalId == LocalId.Value && PublishedPeers == Peers && PublishedMuted == Muted && PublishedBlocked == Blocked)
	{
		return;
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	TSharedRef<SandwichSteam::Voice::FRoutingSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<SandwichSteam::Voice::FRoutingSnapshot, ESPMode::ThreadSafe>();
	Snapshot->LocalId = static_cast<uint64>(LocalId.Value);
	Snapshot->bRecord = bMicOpen;
	Snapshot->bMuteSteamFriendsWhileTalking = Settings && Settings->bMuteSteamFriendsVoiceWhileTalking;
	Snapshot->Peers = Peers;
	Snapshot->Muted.Append(Muted);
	Snapshot->Blocked.Append(Blocked);
	Snapshot->Fake.Count = FakeTalkerCount;
	Snapshot->Fake.DelayMs = FakeTalkerDelayMs;
	Snapshot->Fake.StaggerMs = FakeTalkerStaggerMs;
	Snapshot->Fake.LossPercent = FakeTalkerLossPercent;
	Snapshot->Fake.Channel = FakeTalkerChannel;
	Snapshot->SimLossPercent = SimulatedLossPercent;
	Worker->Publish(Snapshot);

	PublishedPeers = MoveTemp(Peers);
	PublishedMuted = MoveTemp(Muted);
	PublishedBlocked = MoveTemp(Blocked);
	PublishedLocalId = LocalId.Value;
	bPublishedRecord = bMicOpen;
	bRoutingPublished = true;
}

void USteamVoiceSubsystem::DrainWorkerEvents()
{
	if (!Worker.IsValid())
	{
		return;
	}

	using SandwichSteam::Voice::EWorkerEvent;
	SandwichSteam::Voice::FWorkerEvent Event;
	while (Worker->PopEvent(Event))
	{
		const int64 Talker = static_cast<int64>(Event.Talker);
		switch (Event.Type)
		{
		case EWorkerEvent::TalkerAdded:
		{
			FSpeakerSource& Source = SpeakerSources.FindOrAdd(Talker);
			Source.Splitter = Event.Splitter;
			Source.SampleRate = Event.SampleRate;
			Source.bFake = Event.bFake;
			Source.bActive = false;

			// A speaker from an earlier source of this talker reads a dead patch: replace it.
			if (TObjectPtr<USteamVoiceComponent>* Existing = Speakers.Find(Talker))
			{
				if (IsValid(*Existing))
				{
					(*Existing)->DestroyComponent();
				}
				Speakers.Remove(Talker);
			}
			EnsureSpeaker(Talker);
			break;
		}
		case EWorkerEvent::TalkerActive:
			if (FSpeakerSource* Source = SpeakerSources.Find(Talker))
			{
				Source->bActive = true;
			}
			if (USteamVoiceComponent* Speaker = EnsureSpeaker(Talker))
			{
				if (!Speaker->IsPlaying())
				{
					Speaker->Start();
				}
			}
			break;

		case EWorkerEvent::TalkerIdle:
			if (FSpeakerSource* Source = SpeakerSources.Find(Talker))
			{
				Source->bActive = false;
			}
			if (TObjectPtr<USteamVoiceComponent>* Speaker = Speakers.Find(Talker))
			{
				if (IsValid(*Speaker))
				{
					(*Speaker)->Stop();
				}
			}
			break;

		case EWorkerEvent::TalkingStarted:
			PipelineTalking.Add(Talker);
			break;

		case EWorkerEvent::TalkingStopped:
			PipelineTalking.Remove(Talker);
			break;

		case EWorkerEvent::Flood:
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: %s sent too much voice data for 5 seconds and is muted for this session."), *FSteamId(Talker).ToString());
			break;

		case EWorkerEvent::TalkerRemoved:
			SpeakerSources.Remove(Talker);
			PipelineTalking.Remove(Talker);
			if (TObjectPtr<USteamVoiceComponent>* Speaker = Speakers.Find(Talker))
			{
				if (IsValid(*Speaker))
				{
					(*Speaker)->DestroyComponent();
				}
				Speakers.Remove(Talker);
			}
			break;

		default:
			break;
		}
	}
}

USteamVoiceComponent* USteamVoiceSubsystem::EnsureSpeaker(int64 Talker)
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown)
	{
		return nullptr;
	}

	if (SpeakerWorld.Get() != World)
	{
		DestroySpeakers();
		SpeakerWorld = World;
	}

	if (TObjectPtr<USteamVoiceComponent>* Existing = Speakers.Find(Talker))
	{
		if (IsValid(*Existing))
		{
			return *Existing;
		}
		Speakers.Remove(Talker);
	}

	const FSpeakerSource* Source = SpeakerSources.Find(Talker);
	if (!Source || !Source->Splitter.IsValid() || Source->SampleRate <= 0)
	{
		return nullptr;
	}

	// One unattached 2D speaker per talker for now. Pawn speakers and channels come with the full component setup.
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	USteamVoiceComponent* Speaker = NewObject<USteamVoiceComponent>(World, NAME_None, RF_Transient);
	if (Settings && !Settings->VoiceSoundClass.IsNull())
	{
		Speaker->SoundClass = Settings->VoiceSoundClass.LoadSynchronous();
	}
	Speaker->RegisterComponentWithWorld(World);

	// The patch holds up to one second; the speaker keeps it near the playout delay.
	const int32 PlayoutDelayMs = Settings ? Settings->PlayoutDelayMs : 80;
	Speaker->SetVoiceSource(FSteamId(Talker), Source->Splitter->AddNewPatch(Source->SampleRate, 1.0f), Source->SampleRate, PlayoutDelayMs);

	Speakers.Add(Talker, Speaker);
	return Speaker;
}

void USteamVoiceSubsystem::DestroySpeakers()
{
	for (const TPair<int64, TObjectPtr<USteamVoiceComponent>>& Pair : Speakers)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Stop();
			Pair.Value->DestroyComponent();
		}
	}
	Speakers.Reset();
	SpeakerWorld.Reset();
}

void USteamVoiceSubsystem::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	// Speakers live in the world: release them before it goes, or the world would be kept alive (PIE leak check).
	if (World && World == SpeakerWorld.Get())
	{
		DestroySpeakers();
	}
}

// ---- Push to talk ----

bool USteamVoiceSubsystem::TryBindPushToTalk(APlayerController* Controller)
{
	UEnhancedInputComponent* InputComponent = Controller ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (!InputComponent)
	{
		return false; // Not created yet: tried again on the next poll.
	}

	if (BoundComponent.Get() == InputComponent)
	{
		return true;
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	if (!Settings || Settings->PushToTalkAction.IsNull())
	{
		return false;
	}

	const UInputAction* Action = Settings->PushToTalkAction.LoadSynchronous();
	if (!Action)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: the push to talk action '%s' could not be loaded."), *Settings->PushToTalkAction.ToString());
		bPushToTalkUnavailable = true;
		return false;
	}

	UnbindPushToTalk();

	PressedBindingHandle = InputComponent->BindAction(Action, ETriggerEvent::Started, this, &USteamVoiceSubsystem::HandlePushToTalkPressed).GetHandle();
	ReleasedBindingHandle = InputComponent->BindAction(Action, ETriggerEvent::Completed, this, &USteamVoiceSubsystem::HandlePushToTalkReleased).GetHandle();
	CanceledBindingHandle = InputComponent->BindAction(Action, ETriggerEvent::Canceled, this, &USteamVoiceSubsystem::HandlePushToTalkReleased).GetHandle();
	BoundComponent = InputComponent;

	if (!Settings->PushToTalkContext.IsNull())
	{
		const UInputMappingContext* Context = Settings->PushToTalkContext.LoadSynchronous();
		ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
		UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
		if (Context && InputSubsystem)
		{
			if (!InputSubsystem->HasMappingContext(Context))
			{
				InputSubsystem->AddMappingContext(Context, Settings->PushToTalkContextPriority);
				ContextPlayer = LocalPlayer;
				AddedContext = Context;
			}
		}
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: push to talk bound to %s."), *Action->GetName());
	return true;
}

void USteamVoiceSubsystem::UnbindPushToTalk()
{
	if (UEnhancedInputComponent* InputComponent = BoundComponent.Get())
	{
		for (const uint32 Handle : { PressedBindingHandle, ReleasedBindingHandle, CanceledBindingHandle })
		{
			if (Handle != 0)
			{
				InputComponent->RemoveBindingByHandle(Handle);
			}
		}
	}
	PressedBindingHandle = 0;
	ReleasedBindingHandle = 0;
	CanceledBindingHandle = 0;
	BoundComponent.Reset();

	// Only a context this feature added is removed again.
	if (const UInputMappingContext* Context = AddedContext.Get())
	{
		if (ULocalPlayer* LocalPlayer = ContextPlayer.Get())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				InputSubsystem->RemoveMappingContext(Context);
			}
		}
	}
	AddedContext.Reset();
	ContextPlayer.Reset();

	// A key that was held while the binding went away must not keep the microphone open.
	if (bTalkRequested)
	{
		bTalkRequested = false;
		RefreshTalking(false);
	}
}

// ---- Volume ----

bool USteamVoiceSubsystem::ApplyMasterVolume()
{
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!Settings || !World || Settings->VoiceSoundClass.IsNull() || Settings->VoiceSoundMix.IsNull())
	{
		return false;
	}

	USoundClass* SoundClass = Settings->VoiceSoundClass.LoadSynchronous();
	USoundMix* SoundMix = Settings->VoiceSoundMix.LoadSynchronous();
	if (!SoundClass || !SoundMix)
	{
		return false;
	}

	// Nothing to change at the default volume: do not touch the audio system (a mix is only pushed once the volume differs).
	if (FMath::IsNearlyEqual(MasterVolume, 1.0f) && !PushedMix.IsValid())
	{
		return true;
	}

	// A world that is being cleared cannot apply a mix (the audio system logs "RecursiveApplyAdjuster failed"); the next game state applies it.
	if (World->bIsTearingDown)
	{
		return true;
	}

	// The mix has to be active before its class override can be set.
	if (MixWorld.Get() != World || PushedMix.Get() != SoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(World, SoundMix);
		MixWorld = World;
		PushedMix = SoundMix;
	}
	UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, SoundClass, MasterVolume, 1.0f, 0.0f, true);
	return true;
}

void USteamVoiceSubsystem::RemoveVolumeMix()
{
	UWorld* World = MixWorld.Get();
	USoundMix* SoundMix = PushedMix.Get();
	if (World && SoundMix)
	{
		UGameplayStatics::PopSoundMixModifier(World, SoundMix);
	}
	MixWorld.Reset();
	PushedMix.Reset();
}

#if SANDWICHSTEAM_WITH_DEBUG
FSteamResult USteamVoiceSubsystem::SetFakeTalkers(int32 Count, int32 DelayMs, float LossPercent, int32 StaggerMs, int32 Channel)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}
	if (!bUseSteamPipeline)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceFakeNeedsPipeline", "Fake talkers need the Steam voice pipeline (remove bHasVoiceEnabled and [Voice] bEnabled from DefaultEngine.ini)."));
	}

	FakeTalkerCount = FMath::Clamp(Count, 0, SandwichSteam::Voice::MaxFakeTalkers);
	FakeTalkerDelayMs = FMath::Clamp(DelayMs, 0, 2000);
	FakeTalkerStaggerMs = FMath::Clamp(StaggerMs, 0, 2000);
	FakeTalkerLossPercent = FMath::Clamp(LossPercent, 0.0f, 100.0f);
	FakeTalkerChannel = Channel;
	bRoutingPublished = false;
	PublishRouting();
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::SetSimulatedLoss(float LossPercent)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}
	if (!bUseSteamPipeline)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceSimLossNeedsPipeline", "Simulated loss needs the Steam voice pipeline."));
	}

	SimulatedLossPercent = FMath::Clamp(LossPercent, 0.0f, 100.0f);
	bRoutingPublished = false;
	PublishRouting();
	return FSteamResult::Success();
}

FString USteamVoiceSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Voice: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive())
	{
		return Report;
	}

	Report += FString::Printf(TEXT("  Voice: %s, mode %s, microphone %s (requested %s), headset %s\n"),
		bEnabled ? TEXT("enabled") : TEXT("disabled"), Mode == ESteamVoiceMode::PushToTalk ? TEXT("push to talk") : TEXT("open mic"),
		bMicOpen ? TEXT("open") : TEXT("closed"), bTalkRequested ? TEXT("yes") : TEXT("no"), IsHeadsetPresent() ? TEXT("present") : TEXT("not found"));
	Report += FString::Printf(TEXT("  Local user: %s, local talking: %s\n"), *LocalId.ToString(), IsLocalTalking() ? TEXT("yes") : TEXT("no"));

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	Report += FString::Printf(TEXT("  Push to talk: action %s, %s\n"),
		Settings && !Settings->PushToTalkAction.IsNull() ? *Settings->PushToTalkAction.GetAssetName() : TEXT("none"),
		IsPushToTalkBound() ? TEXT("bound") : (bPushToTalkUnavailable ? TEXT("action could not be loaded") : TEXT("not bound")));
	Report += FString::Printf(TEXT("  Master volume: %.2f (sound class %s, mix %s)\n"), MasterVolume,
		Settings && !Settings->VoiceSoundClass.IsNull() ? *Settings->VoiceSoundClass.GetAssetName() : TEXT("none"),
		Settings && !Settings->VoiceSoundMix.IsNull() ? *Settings->VoiceSoundMix.GetAssetName() : TEXT("none"));

	Report += FString::Printf(TEXT("  Players in the game: %d, talking: %d\n"), Present.Num(), Tracker.IsValid() ? Tracker->GetTalking().Num() : 0);
	Report += FString::Printf(TEXT("  Game state: %s, %d player state(s), %d without a unique id, %d with an id that is not a Steam ID%s\n"),
		LastGameState.IsValid() ? TEXT("present") : TEXT("none"), SeenPlayerStates, SeenWithoutNetId, SeenNotSteamId,
		NotSteamSample.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (last: %s)"), *NotSteamSample));
	if (Tracker.IsValid())
	{
		for (const int64 Player : Tracker->GetTalking())
		{
			Report += FString::Printf(TEXT("    talking: %s%s\n"), *FSteamId(Player).ToString(), Player == LocalId.Value ? TEXT(" (you)") : TEXT(""));
		}
	}

	if (Book.IsValid())
	{
		Report += FString::Printf(TEXT("  Muted: %d by the game, %d automatically (blocked on Steam)\n"), Book->NumUserMuted(), Book->NumAutoMuted());
		for (const int64 Player : Book->GetMutedPlayers())
		{
			Report += FString::Printf(TEXT("    muted: %s (%s%s%s)\n"), *FSteamId(Player).ToString(),
				Book->IsUserMuted(Player) ? TEXT("game") : TEXT(""), Book->IsUserMuted(Player) && Book->IsAutoMuted(Player) ? TEXT(" + ") : TEXT(""), Book->IsAutoMuted(Player) ? TEXT("blocked") : TEXT(""));
		}
	}

	Report += FString::Printf(TEXT("  Counters: %d talking events, %d mute call(s) not accepted yet\n"), TalkingEventsSent, MuteCallsFailed);

	Report += FString::Printf(TEXT("  Pipeline: %s\n"), bUseSteamPipeline ? (Worker.IsValid() ? TEXT("Steam voice (running)") : TEXT("Steam voice (stopped)")) : TEXT("Online Subsystem voice"));
	if (Worker.IsValid())
	{
		const SandwichSteam::Voice::FWorkerStats Stats = Worker->GetStats();
		Report += FString::Printf(TEXT("    Worker: %s, loop avg %.2f ms / max %.2f ms, recording %s, last capture result %d\n"),
			Stats.bRunning ? TEXT("running") : TEXT("stopped"), Stats.LoopAverageMs, Stats.LoopMaxMs, Stats.bRecording ? TEXT("yes") : TEXT("no"), Stats.LastCaptureResult);
		Report += FString::Printf(TEXT("    Sample rate: Steam optimal %d Hz, decoding at %d Hz\n"), Stats.OptimalSampleRate, Stats.DecodeSampleRate);
		Report += FString::Printf(TEXT("    Capture: %llu chunk(s), %llu bytes, %llu too large; sent %llu packet(s), %llu failed, %llu hello(s)\n"),
			Stats.CapturedChunks, Stats.CapturedBytes, Stats.CaptureTooLarge, Stats.PacketsSent, Stats.SendFailures, Stats.HellosSent);
		Report += FString::Printf(TEXT("    Payload format: %llu/%llu start with our Steam ID, %llu/%llu end with a valid CRC-32 -> %s\n"),
			Stats.PayloadIdMatches, Stats.CapturedChunks, Stats.PayloadCrcMatches, Stats.CapturedChunks,
			Stats.CapturedChunks == 0 ? TEXT("no capture yet")
			: (Stats.PayloadIdMatches == Stats.CapturedChunks && Stats.PayloadCrcMatches == Stats.CapturedChunks ? TEXT("CONFIRMED") : TEXT("NOT as expected")));
		if (FakeTalkerCount > 0)
		{
			Report += FString::Printf(TEXT("    Fake speaker ids: %llu packet(s) rewritten, %llu kept ours (layout not as expected)\n"), Stats.FakeRewritten, Stats.FakeRewriteFailed);
		}
		Report += FString::Printf(TEXT("    Receive: %llu packet(s), %llu invalid, %llu from non-players, %llu muted, %llu simulated loss, %llu dropped: speaker id is not the sender\n"),
			Stats.PacketsReceived, Stats.Invalid, Stats.FromUnknown, Stats.MutedDrops, Stats.SimLossDrops, Stats.SpeakerMismatch);
		Report += FString::Printf(TEXT("    Sessions: %llu accepted, %llu rejected, %d pending\n"), Stats.SessionsAccepted, Stats.SessionsRejected, Stats.SessionsPending);
		if (FakeTalkerCount > 0 || SimulatedLossPercent > 0.0f)
		{
			Report += FString::Printf(TEXT("    Test: %d fake talker(s), %d ms delay + %d ms per fake, %.0f%% loss; simulated loss %.0f%%\n"),
				FakeTalkerCount, FakeTalkerDelayMs, FakeTalkerStaggerMs, FakeTalkerLossPercent, SimulatedLossPercent);
		}
		if (FakeTalkerCount >= 2)
		{
			// With loss a fake that missed a packet may legitimately decode the next one differently, so the check needs 0% loss.
			const TCHAR* Verdict = Stats.FakeRewriteFailed > 0 ? TEXT("not valid: fake speaker ids could not be rewritten")
				: FakeTalkerLossPercent > 0.0f ? TEXT("not valid with fake loss, use 0% loss")
				: Stats.DecodeCompared < 50 ? TEXT("not enough data yet, keep talking")
				: (Stats.DecodeIdentical == Stats.DecodeCompared ? TEXT("PASS: talkers decode independently")
				: TEXT("FAIL: interleaved talkers change each other's audio (shared decoder state)"));
			Report += FString::Printf(TEXT("    Decoder check: %llu packet(s) compared, %llu identical, %llu length mismatch, max sample difference %d -> %s\n"),
				Stats.DecodeCompared, Stats.DecodeIdentical, Stats.DecodeLengthMismatch, Stats.DecodeMaxDifference, Verdict);
		}

		for (const SandwichSteam::Voice::FTalkerStats& Talker : Stats.Talkers)
		{
			const TObjectPtr<USteamVoiceComponent>* Speaker = Speakers.Find(static_cast<int64>(Talker.Talker));
			const USteamVoiceComponent* Component = Speaker && IsValid(*Speaker) ? Speaker->Get() : nullptr;
			const float BufferedMs = Component ? Component->GetBufferedMs() : 0.0f;

			FString Latency = TEXT("n/a");
			if (Talker.PipelineLatencyMs >= 0.0f)
			{
				Latency = FString::Printf(TEXT("%.0f ms (capture to speaker %.0f + buffer %.0f)"), Talker.PipelineLatencyMs + BufferedMs, Talker.PipelineLatencyMs, BufferedMs);
			}

			Report += FString::Printf(TEXT("    talker %s%s: %s%s, level %.3f, packets %llu, lost %llu, late %llu, dup %llu, resync %llu, flood drops %llu%s, decode errors %llu (last result %d, last %d samples), queued %d\n"),
				*FSteamId(static_cast<int64>(Talker.Talker)).ToString(), Talker.bFake ? TEXT(" (fake)") : TEXT(""),
				Talker.bTalking ? TEXT("TALKING") : TEXT("silent"), Talker.bActive ? TEXT(", speaker on") : TEXT(""),
				Talker.Level, Talker.Packets, Talker.Lost, Talker.Late, Talker.Duplicate, Talker.Resyncs, Talker.FloodDrops,
				Talker.bFlooded ? TEXT(" (MUTED: flood)") : TEXT(""), Talker.DecodeErrors, Talker.LastDecodeResult, Talker.LastDecodedSamples, Talker.Queued);
			Report += FString::Printf(TEXT("      speaker: %s, buffered %.0f ms, underruns %u (incl. ends of speech), latency %s\n"),
				Component ? (Component->IsPlaying() ? TEXT("playing") : TEXT("stopped")) : TEXT("none"), BufferedMs, Component ? Component->GetUnderruns() : 0u, *Latency);
		}
	}
	return Report;
}
#endif
