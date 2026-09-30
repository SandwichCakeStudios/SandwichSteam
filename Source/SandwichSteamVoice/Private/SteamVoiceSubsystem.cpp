// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceSubsystem.h"
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
#include "SteamVoiceBackend.h"
#include "SteamVoiceRules.h"
#include "SteamVoiceSettings.h"

using SandwichSteam::Voice::FMuteBook;
using SandwichSteam::Voice::FTalkingChange;
using SandwichSteam::Voice::FTalkingTracker;

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
	Backend = MakeShared<FSteamVoiceBackend>(GetGameInstance());
	if (!Backend->IsUsable())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: the Steam Online Subsystem has no voice interface. Set [OnlineSubsystem] bHasVoiceEnabled=true and [Voice] bEnabled=true in DefaultEngine.ini (Tools > Sandwich Steam > Configure Steam) and restart."));
		Backend.Reset();
		return false;
	}

	Tracker = MakeUnique<FTalkingTracker>();
	Book = MakeUnique<FMuteBook>();

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	bEnabled = Settings ? Settings->bEnabledByDefault : true;
	Mode = Settings ? Settings->DefaultMode : ESteamVoiceMode::PushToTalk;
	MasterVolume = Settings ? Settings->DefaultMasterVolume : 1.0f;

	LocalId = Backend->GetLocalSteamId();
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

	if (Backend.IsValid() && bMicOpen)
	{
		Backend->StopTalking();
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
	Backend.Reset();
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
	return Backend.IsValid() && Backend->IsHeadsetPresent();
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
		if (!TickHandle.IsValid())
		{
			const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
			const float Interval = Settings ? FMath::Clamp(Settings->TalkingPollSeconds, 0.02f, 1.0f) : 0.1f;
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
	if (!Backend.IsValid())
	{
		return;
	}

	const bool bWant = ShouldTalk();
	if (bWant == bMicOpen && !bForce)
	{
		return;
	}

	bMicOpen = bWant;
	if (bWant)
	{
		Backend->StartTalking();
	}
	else
	{
		Backend->StopTalking();
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

bool USteamVoiceSubsystem::TickVoice(float /*DeltaTime*/)
{
	if (!Backend.IsValid() || !bEnabled)
	{
		TickHandle.Reset();
		return false; // Removes the ticker.
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();

	if (!LocalId.IsValid())
	{
		LocalId = Backend->GetLocalSteamId();
	}

	// A new game state means a new session (or a new map): talkers register again, so what the backend knew is void.
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState != LastGameState.Get())
	{
		LastGameState = GameState;
		Evaluated.Reset();
		Book->ForgetApplied();
		if (GameState)
		{
			ApplyMasterVolume();
			if (bMicOpen)
			{
				Backend->StartTalking(); // The connection is new; open the microphone on it.
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
		if (Backend->IsLocalTalking())
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
			if (Backend->IsRemoteTalking(*NetId))
			{
				NowTalking.Add(Id.Value);
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
			if (Settings && Settings->bAutoMuteBlockedPlayers && Backend->IsBlockedOnSteam(Other.Key) && Book->SetAutoMuted(Other.Key.Value, true))
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
	if (!Backend.IsValid() || !NetId.IsValid() || !Book->NeedsApply(Player.Value))
	{
		return;
	}

	if (Backend->SetMuted(*NetId, Book->IsMuted(Player.Value), Book->IsSystemWide(Player.Value)))
	{
		Book->MarkApplied(Player.Value);
	}
	else
	{
		++MuteCallsFailed; // The talker is probably not registered yet; the next poll tries again.
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
	return Report;
}
#endif
