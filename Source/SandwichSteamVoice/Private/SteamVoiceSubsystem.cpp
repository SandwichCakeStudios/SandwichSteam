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
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Core/SteamSDK.h"
#include "SteamVoiceAuthority.h"
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

	/**
	 * What a Channel speaker keeps buffered. The network jitter is absorbed in the mix (it runs one playout delay ahead), so the speaker only
	 * covers the worker's 10 ms steps and the audio callback.
	 */
	constexpr int32 ChannelSpeakerBufferMs = 40;

	/** Steam voice pipeline: how often the players of the game, their pawns and the push to talk binding are checked (talking is event-driven). */
	constexpr float PipelinePlayerPollSeconds = 0.25f;

	/** Most channels one Transmit or Listen rule may list (rules replicate to everyone). */
	constexpr int32 MaxRuleChannels = 32;

	/** Server: checks a channel rule and drops duplicates (an unrestricted rule keeps no list). False with OutResult set when a channel is not usable. */
	bool NormalizeChannelRule(const FSteamVoiceChannelRule& In, FSteamVoiceChannelRule& Out, FSteamResult& OutResult)
	{
		Out = FSteamVoiceChannelRule();
		if (!In.bRestricted)
		{
			return true;
		}

		Out.bRestricted = true;
		const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
		for (const FSteamVoiceChannel& Channel : In.Channels)
		{
			if (!Channel.IsSet() || Settings->ResolveChannelId(Channel) < 0)
			{
				OutResult = FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, FText::Format(NSLOCTEXT("SandwichSteam", "VoiceRuleChannelInvalid", "The rule channel {0} has neither a tag nor an id, or its tag has no id in Channel Tag Map (Project Settings > Plugins > Sandwich Steam - Voice)."),
					FText::FromString(Channel.Tag.IsValid() ? Channel.Tag.ToString() : FString::FromInt(Channel.Id))));
				return false;
			}
			Out.Channels.AddUnique(Channel);
		}
		if (Out.Channels.Num() > MaxRuleChannels)
		{
			OutResult = FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, FText::Format(NSLOCTEXT("SandwichSteam", "VoiceRuleTooManyChannels", "At most {0} channels per rule."), MaxRuleChannels));
			return false;
		}
		return true;
	}

	FString DescribeChannelRule(const FSteamVoiceChannelRule& Rule)
	{
		if (!Rule.bRestricted)
		{
			return TEXT("any");
		}
		FString Text;
		for (const FSteamVoiceChannel& Channel : Rule.Channels)
		{
			Text += Text.IsEmpty() ? TEXT("") : TEXT(", ");
			Text += Channel.Tag.IsValid() ? Channel.Tag.ToString() : FString::FromInt(Channel.Id);
		}
		return Text.IsEmpty() ? FString(TEXT("none")) : Text;
	}

	FString DescribeRules(const FSteamVoicePlayerRules& Rules)
	{
		const FString Distance = Rules.OwnerVoiceMaxDistance > 0.0f ? FString::Printf(TEXT("%.0f cm"), Rules.OwnerVoiceMaxDistance)
			: (Rules.OwnerVoiceMaxDistance < 0.0f ? FString(TEXT("no limit")) : FString(TEXT("game's")));
		return FString::Printf(TEXT("%s%s, owner voice %s (distance %s), transmit %s, listen %s"), Rules.bCanTalk ? TEXT("may talk") : TEXT("server muted"),
			Rules.bIdentityConflict ? TEXT(", IDENTITY CONFLICT (refused)") : TEXT(""), Rules.bCanUseOwnerVoice ? TEXT("on") : TEXT("off"), *Distance,
			*DescribeChannelRule(Rules.Transmit), *DescribeChannelRule(Rules.Listen));
	}

	/** Tags already reported as missing from the Channel Tag Map. Game thread only. */
	TSet<FGameplayTag> GWarnedUnmappedTags;

	/** The id a channel plays on (Channel Tag Map for tags); INDEX_NONE when unset or unmapped. Warns once per unmapped tag. */
	int32 ResolveChannelId(const FSteamVoiceChannel& Channel)
	{
		const int32 Id = USteamVoiceSettings::Get()->ResolveChannelId(Channel);
		if (Id < 0 && Channel.Tag.IsValid())
		{
			bool bAlreadyWarned = false;
			GWarnedUnmappedTags.Add(Channel.Tag, &bAlreadyWarned);
			if (!bAlreadyWarned)
			{
				UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: channel tag %s has no id in Channel Tag Map (Project Settings > Plugins > Sandwich Steam - Voice); it is not sent or played."), *Channel.Tag.ToString());
			}
		}
		return Id;
	}

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
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();

	// The Online Subsystem voice and the Steam pipeline would both read Steam's microphone: only one of them runs. The setting picks
	// the path and Configure Steam writes the matching ini; when the ini does not match (yet), the path the ini allows runs.
	const bool bWantSteamPipeline = Settings ? Settings->bUseSteamVoicePipeline : true;
	bUseSteamPipeline = !OssFallback->IsUsable();
	if (bUseSteamPipeline != bWantSteamPipeline)
	{
		UE_LOG(LogSandwichSteam, Error, TEXT("Steam voice: Use Steam Voice Pipeline is %s, but DefaultEngine.ini %s [OnlineSubsystem] bHasVoiceEnabled / [Voice] bEnabled. Run Configure Steam and restart; using the %s for now."),
			bWantSteamPipeline ? TEXT("on") : TEXT("off"), bWantSteamPipeline ? TEXT("still has") : TEXT("does not have"),
			bUseSteamPipeline ? TEXT("Steam voice pipeline") : TEXT("Online Subsystem voice"));
	}
	else
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: using the %s."), bUseSteamPipeline ? TEXT("Steam voice pipeline") : TEXT("Online Subsystem voice (fallback)"));
	}
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &USteamVoiceSubsystem::HandleWorldCleanup);

	Tracker = MakeUnique<FTalkingTracker>();
	Book = MakeUnique<FMuteBook>();

	bEnabled = Settings ? Settings->bEnabledByDefault : true;
	bVoiceAllowed = Settings ? Settings->bVoiceAllowedByDefault : true;
	PawnlessVoice = Settings ? Settings->DefaultPawnlessVoice : ESteamVoicePawnlessMode::Play2D;
	Mode =Settings ? Settings->DefaultMode : ESteamVoiceMode::PushToTalk;
	MasterVolume = Settings ? Settings->DefaultMasterVolume : 1.0f;
	ActivationThreshold = Settings ? FMath::Clamp(Settings->VoiceActivationThreshold, 0.0f, 1.0f) : 0.0f;

	LocalId = GetSignedInSteamId(GetGameInstance());
	bTalkRequested = false;
	bMicOpen = false;
	bPushToTalkUnavailable = false;
	LastGameState.Reset();
	Present.Reset();
	Evaluated.Reset();
	TalkingEventsSent = 0;
	MuteCallsFailed = 0;

	// Server rules: the server's own game rules start from the settings; clients take theirs from the authority actor.
	bServerAuthority = Settings && Settings->bServerAuthority && bUseSteamPipeline;
	bServerVoiceAllowed = bVoiceAllowed;
	ServerPawnlessVoice = PawnlessVoice;
	ServerOwnerVoiceMaxDistance = Settings ? FMath::Max(Settings->DefaultOwnerVoiceMaxDistance, 0.0f) : 0.0f;
	ServerTopology = Settings ? Settings->Topology : ESteamVoiceTopology::PeerToPeer;
	IdentityConflicts.Reset();
	LocalChannelMapHash = Settings ? SandwichSteam::Voice::HashChannelMap(Settings->ChannelTagMap) : 0;
	bRulesReceived = false;
	AppliedRulesVersion = 0;
	AppliedPlayerRules.Reset();
	ResolvedRules.Reset();
	OwnerVoiceMaxDistance = 0.0f;
	bChannelMapMismatch = false;
	MouthInRange.Reset();
	MouthNotSent.Reset();
	MouthDropped.Reset();
	AppliedRelayHost = 0;
	RelayMouthInRange.Reset();
	RelayMouthOutOfRange.Reset();
	if (Settings && Settings->Topology == ESteamVoiceTopology::ServerRelay && !bServerAuthority)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: Topology is Server Relay, but Server Authority is off (or the Online Subsystem voice runs): voice runs peer to peer."));
	}
	if (bServerAuthority)
	{
		ActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddWeakLambda(this, [this](const UWorld::FActorsInitializedParams& Params)
		{
			EnsureAuthority(Params.World);
		});

		// The feature can start after the world (Steam became ready later): its authority is set up now.
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			EnsureAuthority(GameInstance->GetWorld());
		}
	}
	else if (Settings && Settings->bServerAuthority)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: Server Authority needs the Steam voice pipeline; the Online Subsystem voice runs without server rules."));
	}

	if (bEnabled)
	{
		ApplyEnabledState();
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: %s%s, %s."), bEnabled ? TEXT("enabled") : TEXT("disabled (Enabled By Default is off)"),
		bVoiceAllowed ? TEXT("") : TEXT(", not allowed by the game (Voice Allowed By Default is off)"), Mode == ESteamVoiceMode::PushToTalk ? TEXT("push to talk") : TEXT("open mic"));
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
	RegisteredSpeakers.Reset();
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}

	// The authority actor stays in its world: a feature that starts again finds it there.
	if (ActorsInitializedHandle.IsValid())
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(ActorsInitializedHandle);
		ActorsInitializedHandle.Reset();
	}
	Authority.Reset();
	bServerAuthority = false;
	bRulesReceived = false;
	AppliedRulesVersion = 0;
	AppliedPlayerRules.Reset();
	ResolvedRules.Reset();
	OwnerVoiceMaxDistance = 0.0f;
	bChannelMapMismatch = false;
	IdentityConflicts.Reset();
	MouthInRange.Reset();
	MouthNotSent.Reset();
	MouthDropped.Reset();
	AppliedRelayHost = 0;
	RelayMouthInRange.Reset();
	RelayMouthOutOfRange.Reset();
	bRelayEcho = false;

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

FSteamResult USteamVoiceSubsystem::SetVoiceAllowed(bool bAllowed)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	// Server Authority: the server's value reaches every client (and this one) through the authority actor.
	if (bServerAuthority)
	{
		if (!IsServerWorld())
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_WrongScope, NSLOCTEXT("SandwichSteam", "VoiceAllowedServerOnly", "Server Authority is on: only the server sets Voice Allowed. It reaches every client with the voice rules."));
		}
		if (bAllowed != bServerVoiceAllowed)
		{
			bServerVoiceAllowed = bAllowed;
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: the server %s voice for every player."), bAllowed ? TEXT("allows") : TEXT("does not allow"));
			PushServerGameRules();
		}
		return FSteamResult::Success();
	}

	if (bAllowed != bVoiceAllowed)
	{
		bVoiceAllowed = bAllowed;
		RefreshTalking(false);
		PublishRouting(); // No peers while not allowed: nothing is sent, voice from others is dropped and their talkers go away.
		OnVoiceAllowedChanged.Broadcast(bVoiceAllowed);
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: %s by the game."), bVoiceAllowed ? TEXT("allowed") : TEXT("not allowed"));
	}
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::SetPawnlessVoice(ESteamVoicePawnlessMode NewMode)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (bServerAuthority)
	{
		if (!IsServerWorld())
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_WrongScope, NSLOCTEXT("SandwichSteam", "VoicePawnlessServerOnly", "Server Authority is on: only the server sets Pawnless Voice. It reaches every client with the voice rules."));
		}
		if (NewMode != ServerPawnlessVoice)
		{
			ServerPawnlessVoice = NewMode;
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: the server makes players without a pawn %s for every player."), NewMode == ESteamVoicePawnlessMode::Silent ? TEXT("silent") : TEXT("heard in 2D"));
			PushServerGameRules();
		}
		return FSteamResult::Success();
	}

	if (NewMode != PawnlessVoice)
	{
		PawnlessVoice = NewMode;
		bSpeakersDirty = true;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: players without a pawn are now %s."), PawnlessVoice == ESteamVoicePawnlessMode::Silent ? TEXT("silent") : TEXT("heard in 2D"));
	}
	if (!bUseSteamPipeline)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoicePawnlessNeedsPipeline", "Pawnless voice needs the Steam voice pipeline (Use Steam Voice Pipeline in the Voice settings)."));
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
		// Steam has no headset query. The capture says NotInitialized when voice capture failed to start; a machine without a
		// microphone still reports NoData, exactly like silence, so this stays true there.
		return Worker.IsValid() && Worker->GetStats().LastCaptureResult != static_cast<int32>(k_EVoiceResultNotInitialized);
#else
		return false;
#endif
	}
	return OssFallback.IsValid() && OssFallback->IsHeadsetPresent();
}

FSteamResult USteamVoiceSubsystem::SetVoiceActivationThreshold(float Threshold)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	ActivationThreshold = FMath::Clamp(Threshold, 0.0f, 1.0f);
	if (!bUseSteamPipeline)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceActivationNeedsPipeline", "Voice activation needs the Steam voice pipeline (Use Steam Voice Pipeline in the Voice settings)."));
	}
	if (Worker.IsValid())
	{
		Worker->SetActivationThreshold(ActivationThreshold);
	}
	return FSteamResult::Success();
}

float USteamVoiceSubsystem::GetLocalMicLevel() const
{
	return Worker.IsValid() ? Worker->GetLocalLevel() : 0.0f;
}

FSteamResult USteamVoiceSubsystem::StartMicTest()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}
	if (!bUseSteamPipeline)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceMicTestNeedsPipeline", "The mic test needs the Steam voice pipeline (Use Steam Voice Pipeline in the Voice settings)."));
	}
	if (!bEnabled)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "VoiceMicTestDisabled", "Voice is disabled. Call Set Voice Enabled first."));
	}
	if (!Worker.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "VoiceMicTestNoPipeline", "The Steam voice pipeline is not running (another game instance in this process owns the microphone)."));
	}

	bMicTest = true;
	PublishRouting();
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::StopMicTest()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	bMicTest = false;
	PublishRouting();
	return FSteamResult::Success();
}

bool USteamVoiceSubsystem::CanSetTransmit(FSteamResult& OutResult) const
{
	if (!RequireActive(OutResult))
	{
		return false;
	}
	if (!bUseSteamPipeline)
	{
		OutResult = FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceTransmitNeedsPipeline", "Transmit channels need the Steam voice pipeline (Use Steam Voice Pipeline in the Voice settings)."));
		return false;
	}
	return true;
}

FSteamResult USteamVoiceSubsystem::SetTransmitChannels(const TArray<FSteamVoiceChannel>& Channels)
{
	FSteamResult Result;
	if (!CanSetTransmit(Result))
	{
		return Result;
	}

	TArray<FSteamVoiceChannel> Unique;
	Unique.Reserve(Channels.Num());
	for (const FSteamVoiceChannel& Channel : Channels)
	{
		if (!Channel.IsSet())
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceTransmitChannelNotSet", "A transmit channel has neither a tag nor an id."));
		}
		if (ResolveChannelId(Channel) < 0)
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, FText::Format(NSLOCTEXT("SandwichSteam", "VoiceTransmitChannelUnmapped", "The channel tag {0} has no id in Channel Tag Map (Project Settings > Plugins > Sandwich Steam - Voice)."), FText::FromString(Channel.Tag.ToString())));
		}
		Unique.AddUnique(Channel);
	}
	if (Unique.Num() > SandwichSteam::Voice::MaxPacketChannels)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, FText::Format(NSLOCTEXT("SandwichSteam", "VoiceTransmitTooManyChannels", "At most {0} transmit channels."), SandwichSteam::Voice::MaxPacketChannels));
	}

	TransmitChannels = MoveTemp(Unique);
	PublishRouting();
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::AddTransmitChannel(const FSteamVoiceChannel& Channel)
{
	if (TransmitChannels.Contains(Channel))
	{
		FSteamResult Result;
		return CanSetTransmit(Result) ? FSteamResult::Success() : Result;
	}

	TArray<FSteamVoiceChannel> Channels = TransmitChannels;
	Channels.Add(Channel);
	return SetTransmitChannels(Channels);
}

FSteamResult USteamVoiceSubsystem::RemoveTransmitChannel(const FSteamVoiceChannel& Channel)
{
	TArray<FSteamVoiceChannel> Channels = TransmitChannels;
	Channels.Remove(Channel);
	return SetTransmitChannels(Channels);
}

FSteamResult USteamVoiceSubsystem::ClearTransmitChannels()
{
	return SetTransmitChannels(TArray<FSteamVoiceChannel>());
}

FSteamResult USteamVoiceSubsystem::SetTransmitToOwner(bool bTransmit)
{
	FSteamResult Result;
	if (!CanSetTransmit(Result))
	{
		return Result;
	}

	bTransmitToOwner = bTransmit;
	PublishRouting();
	return FSteamResult::Success();
}

TArray<int32> USteamVoiceSubsystem::ResolveTransmitChannels() const
{
	TArray<int32> Ids;
	Ids.Reserve(TransmitChannels.Num());
	for (const FSteamVoiceChannel& Channel : TransmitChannels)
	{
		const int32 Id = ResolveChannelId(Channel);
		if (Id >= 0)
		{
			Ids.AddUnique(Id);
		}
	}
	Ids.Sort();
	return Ids;
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

FSteamResult USteamVoiceSubsystem::SetPlayerVolume(FSteamId Player, float Volume)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}
	if (!bUseSteamPipeline)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoicePlayerVolumeNeedsPipeline", "Per player volume needs the Steam voice pipeline (Use Steam Voice Pipeline in the Voice settings)."));
	}
	if (!Player.IsValid() || Player == LocalId)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoicePlayerVolumeInvalid", "Pass the Steam ID of another player."));
	}

	const float Clamped = FMath::Clamp(Volume, 0.0f, 2.0f);
	const float Previous = GetPlayerVolume(Player);
	if (Clamped == Previous)
	{
		return FSteamResult::Success();
	}

	if (Clamped == 1.0f)
	{
		PlayerVolumes.Remove(Player.Value);
	}
	else
	{
		PlayerVolumes.Add(Player.Value, Clamped);
	}
	bPlayerVolumesChanged = true;
	PublishRouting();
	return FSteamResult::Success();
}

float USteamVoiceSubsystem::GetPlayerVolume(FSteamId Player) const
{
	const float* Volume = PlayerVolumes.Find(Player.Value);
	return Volume ? *Volume : 1.0f;
}

float USteamVoiceSubsystem::GetPlayerVoiceLevel(FSteamId Player) const
{
	if (!Worker.IsValid() || !Player.IsValid())
	{
		return 0.0f;
	}
	return Player == LocalId ? Worker->GetLocalLevel() : Worker->GetTalkerLevel(static_cast<uint64>(Player.Value));
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
		EnsureTicker();

		bPushToTalkUnavailable = false;
		LastGameState.Reset(); // The next poll treats the current game as a new session: mutes are applied again.
		ApplyMasterVolume();
		RefreshTalking(true);
		return;
	}

	// A relay host keeps forwarding everybody else's voice: only this player's side stops (no microphone, nothing played or reported).
	const bool bRelayOnly = IsVoiceRelayHost();
	if (!bRelayOnly && TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	bTalkRequested = false;
	bMicTest = false;
	UnbindPushToTalk();
	RefreshTalking(true);
	RemoveVolumeMix();
	if (bRelayOnly)
	{
		StartPipeline(); // Publishes the routing without local playback; the worker lets go of every speaker.
		EnsureTicker();
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: voice is off for this player; this host keeps relaying the other players' voice."));
	}
	else
	{
		StopPipeline();
	}

	if (Tracker.IsValid())
	{
		TArray<FTalkingChange> Stopped;
		Tracker->Clear(Stopped);
		for (const FTalkingChange& Change : Stopped)
		{
			BroadcastTalking(Change.Player, false);
		}
	}
	if (!bRelayOnly)
	{
		Present.Reset();
		Evaluated.Reset();
	}
}

void USteamVoiceSubsystem::EnsureTicker()
{
	if (TickHandle.IsValid())
	{
		return;
	}

	// The pipeline ticks every frame to handle the worker's events on time; its player poll runs every PipelinePlayerPollSeconds.
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	const float Interval = bUseSteamPipeline ? 0.0f : (Settings ? FMath::Clamp(Settings->TalkingPollSeconds, 0.02f, 1.0f) : 0.1f);
	PollAccumulator = TNumericLimits<float>::Max(); // Poll on the first tick.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USteamVoiceSubsystem::TickVoice), Interval);
}

bool USteamVoiceSubsystem::ShouldTalk() const
{
	return bEnabled && IsVoiceAllowedByRules() && CanLocalTalkByRules() && (Mode == ESteamVoiceMode::OpenMic || bTalkRequested);
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
	if (!OssFallback.IsValid() || !ShouldRunPipeline())
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

		// A new world, or speakers / pawns changed: connect the talkers to their speakers again.
		if (Worker.IsValid() && (bSpeakersDirty || (!SpeakerSources.IsEmpty() && SpeakerWorld.Get() != World)))
		{
			UpdateSpeakers();
		}

		// Talking comes from the worker's events; the poll only follows the players, their pawns and the push to talk binding.
		PollAccumulator += DeltaTime;
		if (PollAccumulator < PipelinePlayerPollSeconds)
		{
			return true;
		}
		PollAccumulator = 0.0f;
	}

	if (!LocalId.IsValid())
	{
		LocalId = GetSignedInSteamId(GetGameInstance());
		if (LocalId.IsValid() && HasVoiceAuthority() && ServerTopology == ESteamVoiceTopology::ServerRelay)
		{
			PushServerGameRules(); // Server Relay waited for this host's Steam ID.
		}
	}

	// A new game state means a new session (or a new map): talkers register again, so what the Online Subsystem knew is void.
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState != LastGameState.Get())
	{
		LastGameState = GameState;
		Evaluated.Reset();
		Book->ForgetApplied();
		if (GameState && bEnabled) // A relay host with voice off has no volume mix.
		{
			ApplyMasterVolume();
			if (bMicOpen && !bUseSteamPipeline)
			{
				OssFallback->StartTalking(); // The connection is new; open the microphone on it.
			}
		}
	}

	// Push to talk binding: the controller and its input component appear some time after the map loaded.
	if (bEnabled && Settings && Settings->bAutoBindPushToTalk && !bPushToTalkUnavailable && !Settings->PushToTalkAction.IsNull() && !IsPushToTalkBound())
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
		if (!bUseSteamPipeline && OssFallback->IsLocalTalking())
		{
			NowTalking.Add(LocalId.Value);
		}
	}

	SeenPlayerStates = 0;
	SeenWithoutNetId = 0;
	SeenNotSteamId = 0;
	// Server: every player's Steam ID in player array order (the joining player last), for the identity check.
	const bool bCheckIdentity = HasVoiceAuthority();
	TArray<int64, TInlineAllocator<16>> AllIds;
	TArray<APlayerState*, TInlineAllocator<16>> AllStates;
	if (GameState)
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
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
			if (bCheckIdentity)
			{
				AllIds.Add(Id.Value);
				AllStates.Add(PlayerState);
			}
			if (Id == LocalId)
			{
				continue;
			}

			NowPresent.Add(Id.Value);
			Others.Emplace(Id, NetId);
			if (Worker.IsValid())
			{
				TrackPlayerState(Id.Value, PlayerState);
			}
			if (!bUseSteamPipeline && OssFallback->IsRemoteTalking(*NetId))
			{
				NowTalking.Add(Id.Value);
			}
		}
	}

	// Fake talkers (Steam.Voice.FakeTalker) count as present, so the test panel lists them. The mic test talker is no player.
	for (const TPair<int64, FSpeakerSource>& Source : SpeakerSources)
	{
		if (Source.Value.bFake && Source.Key != static_cast<int64>(SandwichSteam::Voice::MicTestTalkerId))
		{
			NowPresent.Add(Source.Key);
		}
	}
	Present = MoveTemp(NowPresent);

	if (Worker.IsValid())
	{
		for (auto It = PlayerStates.CreateIterator(); It; ++It)
		{
			if (!Present.Contains(It.Key()))
			{
				if (APlayerState* PlayerState = It.Value().Get())
				{
					PlayerState->OnPawnSet.RemoveDynamic(this, &USteamVoiceSubsystem::HandlePawnSet);
				}
				It.RemoveCurrent();
				bSpeakersDirty = true;
			}
		}

		// Fallback for OnPawnSet: a talker whose pawn changed since the last speaker update.
		for (const TPair<int64, FSpeakerSource>& Source : SpeakerSources)
		{
			const TWeakObjectPtr<APlayerState>* PlayerState = PlayerStates.Find(Source.Key);
			const APawn* Pawn = PlayerState && PlayerState->IsValid() ? (*PlayerState)->GetPawn() : nullptr;
			if (Pawn != Source.Value.Pawn.Get())
			{
				bSpeakersDirty = true;
				break;
			}
		}
	}

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

	// The pipeline's talking is event-driven (DrainWorkerEvents); a talker who leaves gets a stop from the worker.
	if (!bUseSteamPipeline)
	{
		TArray<FTalkingChange> Changes;
		Tracker->Update(NowTalking, Present, Changes);
		for (const FTalkingChange& Change : Changes)
		{
			BroadcastTalking(Change.Player, Change.bTalking);
		}
	}

	if (bCheckIdentity)
	{
		UpdateIdentityConflicts(AllIds, AllStates);
	}
	UpdateOwnerVoiceRange();
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
		Config.ActivationHoldSeconds = static_cast<float>(FMath::Max(Settings->VoiceActivationHoldMs, 0)) / 1000.0f;
		Config.ChannelLeadSeconds = static_cast<float>(FMath::Clamp(Settings->PlayoutDelayMs, 0, 500)) / 1000.0f;
	}
	Config.ActivationThreshold = ActivationThreshold;

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
	ChannelOutputs.Reset();
	ForgetPlayerStates();
	ClearPlayerTransmits();
	bRoutingPublished = false;
}

void USteamVoiceSubsystem::PublishRouting()
{
	if (!Worker.IsValid())
	{
		return;
	}

	// Not allowed by the game, or the server's rules not here yet: no peers, so the worker sends nothing and drops voice from everyone
	// (the mic test still plays back).
	TArray<uint64> Peers;
	if (IsVoiceAllowedByRules())
	{
		Peers.Reserve(Present.Num());
		for (const int64 Player : Present)
		{
			if (Player != LocalId.Value && !SandwichSteam::Voice::IsFakeTalkerId(static_cast<uint64>(Player)) && static_cast<uint64>(Player) != SandwichSteam::Voice::RelayEchoTalkerId)
			{
				Peers.Add(static_cast<uint64>(Player));
			}
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
	// Our own server rules decide what we send at all (no owner voice, allowed transmit channels); receivers check it again.
	static const SandwichSteam::Voice::FPlayerRules DefaultRules;
	const SandwichSteam::Voice::FPlayerRules* FoundLocalRules = LocalId.IsValid() ? ResolvedRules.Find(LocalId.Value) : nullptr;
	const SandwichSteam::Voice::FPlayerRules& LocalRules = FoundLocalRules ? *FoundLocalRules : DefaultRules;
	const TArray<int32> Wanted = ResolveTransmitChannels();
	SandwichSteam::Voice::FVoiceRoute LocalRoute;
	SandwichSteam::Voice::FilterForTalker(LocalRules, bTransmitToOwner, Wanted, LocalRoute);
	const bool bTransmitMouth = LocalRoute.bMouth;
	TArray<int32> Transmit(LocalRoute.Channels.GetData(), LocalRoute.Channels.Num());

	// Server rule changes force a publish (bRoutingPublished = false), so they need no comparison here.
	if (bRoutingPublished && !bChannelOutputsChanged && !bPlayerVolumesChanged && bPublishedRecord == bMicOpen && bPublishedMicTest == bMicTest && PublishedLocalId == LocalId.Value && PublishedPeers == Peers && PublishedMuted == Muted && PublishedBlocked == Blocked
		&& bPublishedTransmitMouth == bTransmitMouth && PublishedTransmitChannels == Transmit && PublishedMouthNotSent == MouthNotSent && PublishedMouthDropped == MouthDropped
		&& PublishedRelayHost == AppliedRelayHost && bPublishedLocalPlayback == bEnabled && PublishedRelayMouthOutOfRange.OrderIndependentCompareEqual(RelayMouthOutOfRange))
	{
		return;
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	TSharedRef<SandwichSteam::Voice::FRoutingSnapshot, ESPMode::ThreadSafe> Snapshot = MakeShared<SandwichSteam::Voice::FRoutingSnapshot, ESPMode::ThreadSafe>();
	Snapshot->LocalId = static_cast<uint64>(LocalId.Value);
	Snapshot->bRecord = bMicOpen || bMicTest;
	Snapshot->bSend = bMicOpen;
	Snapshot->bMicTest = bMicTest;
	Snapshot->bMuteSteamFriendsWhileTalking = Settings && Settings->bMuteSteamFriendsVoiceWhileTalking;
	Snapshot->bTransmitMouth = bTransmitMouth;
	Snapshot->TransmitChannels = Transmit;
	Snapshot->Peers = Peers;
	Snapshot->Muted.Append(Muted);
	Snapshot->bReportMutedTalking = Settings && Settings->bReportMutedTalking;
	Snapshot->Blocked.Append(Blocked);
	Snapshot->Rules.Reserve(ResolvedRules.Num());
	for (const TPair<int64, SandwichSteam::Voice::FPlayerRules>& Pair : ResolvedRules)
	{
		Snapshot->Rules.Add(static_cast<uint64>(Pair.Key), Pair.Value);
	}
	Snapshot->LocalRules = LocalRules;
	Snapshot->MouthNotSent.Append(MouthNotSent);
	Snapshot->MouthDropped.Append(MouthDropped);
	Snapshot->RelayHost = static_cast<uint64>(AppliedRelayHost);
	Snapshot->bLocalPlayback = bEnabled;
	if (IsVoiceRelayHost())
	{
		Snapshot->RelayMouthOutOfRange = RelayMouthOutOfRange;
		Snapshot->RelayFilter = RelayFilter;
		Snapshot->bRelayEcho = bRelayEcho;
	}
	Snapshot->PlayerVolumes.Reserve(PlayerVolumes.Num());
	for (const TPair<int64, float>& Pair : PlayerVolumes)
	{
		Snapshot->PlayerVolumes.Add(static_cast<uint64>(Pair.Key), Pair.Value);
	}
	Snapshot->Fake.Count = FakeTalkerCount;
	Snapshot->Fake.DelayMs = FakeTalkerDelayMs;
	Snapshot->Fake.StaggerMs = FakeTalkerStaggerMs;
	Snapshot->Fake.LossPercent = FakeTalkerLossPercent;
	Snapshot->Fake.Channel = FakeTalkerChannel;
	Snapshot->SimLossPercent = SimulatedLossPercent;
	Snapshot->ChannelOutputs.Reserve(ChannelOutputs.Num());
	for (const TPair<FChannelOutputKey, FChannelOutputState>& Pair : ChannelOutputs)
	{
		SandwichSteam::Voice::FChannelOutput& Output = Snapshot->ChannelOutputs.AddDefaulted_GetRef();
		Output.Channel = Pair.Key.Channel;
		Output.Excluded = static_cast<uint64>(Pair.Key.Excluded);
		Output.Splitter = Pair.Value.Splitter;
	}
	Snapshot->ChannelOutputs.Sort([](const SandwichSteam::Voice::FChannelOutput& A, const SandwichSteam::Voice::FChannelOutput& B)
	{
		return A.Channel != B.Channel ? A.Channel < B.Channel : A.Excluded < B.Excluded;
	});
	Worker->Publish(Snapshot);
	bChannelOutputsChanged = false;
	bPlayerVolumesChanged = false;

	PublishedPeers = MoveTemp(Peers);
	PublishedMuted = MoveTemp(Muted);
	PublishedBlocked = MoveTemp(Blocked);
	PublishedTransmitChannels = MoveTemp(Transmit);
	PublishedMouthNotSent = MouthNotSent;
	PublishedMouthDropped = MouthDropped;
	PublishedRelayMouthOutOfRange = RelayMouthOutOfRange;
	PublishedRelayHost = AppliedRelayHost;
	bPublishedLocalPlayback = bEnabled;
	bPublishedTransmitMouth = bTransmitMouth;
	PublishedLocalId = LocalId.Value;
	bPublishedRecord = bMicOpen;
	bPublishedMicTest = bMicTest;
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

		// The mic test talker only needs its speaker: it is the local voice (which has its own talking events), not a player.
		if (Event.Talker == SandwichSteam::Voice::MicTestTalkerId
			&& (Event.Type == EWorkerEvent::TalkingStarted || Event.Type == EWorkerEvent::TalkingStopped || Event.Type == EWorkerEvent::TransmitChanged))
		{
			continue;
		}

		switch (Event.Type)
		{
		case EWorkerEvent::TalkerAdded:
		{
			FSpeakerSource& Source = SpeakerSources.FindOrAdd(Talker);
			// Speakers of an earlier source of this talker read a dead patch: connect them again.
			DisconnectSpeakers(Talker, Source);
			Source.Splitter = Event.Splitter;
			Source.SampleRate = Event.SampleRate;
			Source.bFake = Event.bFake;
			Source.bActive = false;
			bSpeakersDirty = true;
			break;
		}
		case EWorkerEvent::TalkerActive:
			if (FSpeakerSource* Source = SpeakerSources.Find(Talker))
			{
				Source->bActive = true;
				for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Source->Connected)
				{
					if (Speaker.IsValid() && !Speaker->IsPlaying())
					{
						Speaker->Start();
					}
				}
			}
			break;

		case EWorkerEvent::TalkerIdle:
			if (FSpeakerSource* Source = SpeakerSources.Find(Talker))
			{
				Source->bActive = false;
				for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Source->Connected)
				{
					if (Speaker.IsValid())
					{
						Speaker->Stop();
					}
				}
			}
			break;

		case EWorkerEvent::ChannelActive:
		case EWorkerEvent::ChannelIdle:
		{
			// Only for the mix the event is about: a mix created again since then has its own events.
			FChannelOutputState* Output = ChannelOutputs.Find(FChannelOutputKey{ Event.Channel, Talker });
			if (!Output || Output->Splitter != Event.Splitter)
			{
				break;
			}

			Output->bActive = Event.Type == EWorkerEvent::ChannelActive;
			for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Output->Connected)
			{
				if (!Speaker.IsValid())
				{
					continue;
				}
				if (!Output->bActive)
				{
					Speaker->Stop();
				}
				else if (!Speaker->IsPlaying())
				{
					Speaker->Start();
				}
			}
			break;
		}

		case EWorkerEvent::TalkingStarted:
		case EWorkerEvent::TalkingStopped:
		{
			const bool bTalking = Event.Type == EWorkerEvent::TalkingStarted;
			if (Tracker.IsValid() && Tracker->Set(Talker, bTalking))
			{
				BroadcastTalking(Talker, bTalking);
			}
			break;
		}

		case EWorkerEvent::TransmitChanged:
		{
			FPlayerTransmit* Previous = PlayerTransmits.Find(Talker);
			const bool bChannelsChanged = Previous ? Previous->Channels != Event.Channels : !Event.Channels.IsEmpty();
			if (!Event.bMouth && Event.Channels.IsEmpty())
			{
				PlayerTransmits.Remove(Talker);
			}
			else
			{
				FPlayerTransmit& Transmit = PlayerTransmits.FindOrAdd(Talker);
				Transmit.bMouth = Event.bMouth;
				Transmit.Channels = MoveTemp(Event.Channels);
			}

			if (bChannelsChanged)
			{
				const FPlayerTransmit* Current = PlayerTransmits.Find(Talker);
				OnPlayerTransmitChannelsChanged.Broadcast(FSteamId(Talker), Current ? MakeChannels(Current->Channels) : TArray<FSteamVoiceChannel>());
			}
			break;
		}

		case EWorkerEvent::Flood:
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: %s sent too much voice data for 5 seconds and is muted for this session."), *FSteamId(Talker).ToString());
			OnPlayerVoiceFlood.Broadcast(FSteamId(Talker));
			break;

		case EWorkerEvent::RelayChanged:
			OnServerVoiceRelayChanged.Broadcast(FSteamId(Talker), Event.bMouth || !Event.Channels.IsEmpty(), Event.bMouth, MakeChannels(Event.Channels));
			break;

		case EWorkerEvent::TalkerRemoved:
			if (FSpeakerSource* Source = SpeakerSources.Find(Talker))
			{
				DisconnectSpeakers(Talker, *Source);
			}
			SpeakerSources.Remove(Talker);
			DestroyPluginSpeaker(AutoSpeakers, Talker);
			DestroyPluginSpeaker(FallbackSpeakers, Talker);
			break;

		default:
			break;
		}
	}

	// Connect new talkers in the same frame, so their first word is kept.
	if (bSpeakersDirty)
	{
		UpdateSpeakers();
	}
}

TArray<FSteamId> USteamVoiceSubsystem::GetSpeakerTalkers(const USteamVoiceComponent* Speaker) const
{
	TArray<FSteamId> Talkers;
	if (!Speaker || !Speaker->HasVoiceSource())
	{
		return Talkers;
	}

	// The local voice never reaches a speaker; a muted player is only listed with Report Muted Talking, and never heard.
	const auto IsHeard = [this](int64 Player)
	{
		return Player != LocalId.Value && !(Book.IsValid() && Book->IsMuted(Player));
	};

	if (Speaker->Source == ESteamVoiceSpeakerSource::Owner)
	{
		const int64 Player = Speaker->GetTalker().Value;
		const FPlayerTransmit* Transmit = PlayerTransmits.Find(Player);
		if (Transmit && Transmit->bMouth && IsHeard(Player))
		{
			Talkers.Add(FSteamId(Player));
		}
		return Talkers;
	}

	for (const TPair<FChannelOutputKey, FChannelOutputState>& Output : ChannelOutputs)
	{
		if (!Output.Value.Connected.Contains(Speaker))
		{
			continue;
		}
		for (const TPair<int64, FPlayerTransmit>& Pair : PlayerTransmits)
		{
			if (Pair.Key != Output.Key.Excluded && Pair.Value.Channels.Contains(Output.Key.Channel) && IsHeard(Pair.Key))
			{
				Talkers.Add(FSteamId(Pair.Key));
			}
		}
		break;
	}
	return Talkers;
}

TArray<FSteamVoiceChannel> USteamVoiceSubsystem::GetPlayerTransmitChannels(FSteamId Player) const
{
	const FPlayerTransmit* Transmit = PlayerTransmits.Find(Player.Value);
	return Transmit ? MakeChannels(Transmit->Channels) : TArray<FSteamVoiceChannel>();
}

bool USteamVoiceSubsystem::IsPlayerTransmittingOn(FSteamId Player, const FSteamVoiceChannel& Channel) const
{
	const FPlayerTransmit* Transmit = PlayerTransmits.Find(Player.Value);
	const int32 Id = Transmit ? USteamVoiceSettings::Get()->ResolveChannelId(Channel) : INDEX_NONE;
	return Id >= 0 && Transmit->Channels.Contains(Id);
}

TArray<FSteamVoiceChannel> USteamVoiceSubsystem::MakeChannels(TConstArrayView<int32> Ids)
{
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	TArray<FSteamVoiceChannel> Channels;
	Channels.Reserve(Ids.Num());
	for (const int32 Id : Ids)
	{
		bool bTagged = false;
		if (Settings)
		{
			for (const TPair<FGameplayTag, int32>& Pair : Settings->ChannelTagMap)
			{
				if (Pair.Value == Id && Pair.Key.IsValid())
				{
					FSteamVoiceChannel& Channel = Channels.AddDefaulted_GetRef();
					Channel.Tag = Pair.Key;
					bTagged = true;
				}
			}
		}
		if (!bTagged)
		{
			FSteamVoiceChannel& Channel = Channels.AddDefaulted_GetRef();
			Channel.Id = Id;
		}
	}
	return Channels;
}

void USteamVoiceSubsystem::ClearPlayerTransmits()
{
	TArray<int64> WithChannels;
	for (const TPair<int64, FPlayerTransmit>& Pair : PlayerTransmits)
	{
		if (!Pair.Value.Channels.IsEmpty())
		{
			WithChannels.Add(Pair.Key);
		}
	}
	PlayerTransmits.Reset();
	for (const int64 Player : WithChannels)
	{
		OnPlayerTransmitChannelsChanged.Broadcast(FSteamId(Player), TArray<FSteamVoiceChannel>());
	}
}

TArray<USteamVoiceComponent*> USteamVoiceSubsystem::GetPlayerVoiceComponents(FSteamId Player) const
{
	TArray<USteamVoiceComponent*> Result;
	if (const FSpeakerSource* Source = SpeakerSources.Find(Player.Value))
	{
		for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Source->Connected)
		{
			if (USteamVoiceComponent* Component = Speaker.Get())
			{
				Result.Add(Component);
			}
		}
	}
	return Result;
}

void USteamVoiceSubsystem::RegisterSpeaker(USteamVoiceComponent* Speaker)
{
	if (!IsValid(Speaker))
	{
		return;
	}

	if (!Speaker->SoundClass)
	{
		const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
		if (Settings && !Settings->VoiceSoundClass.IsNull())
		{
			Speaker->SoundClass = Settings->VoiceSoundClass.LoadSynchronous();
		}
	}

	RegisteredSpeakers.Add(Speaker);
	bSpeakersDirty = true;
}

void USteamVoiceSubsystem::UnregisterSpeaker(USteamVoiceComponent* Speaker)
{
	// The component clears its own voice source; the next update drops it from its talker.
	if (RegisteredSpeakers.Remove(Speaker) > 0)
	{
		bSpeakersDirty = true;
	}
}

void USteamVoiceSubsystem::UpdateSpeakers()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->bIsTearingDown)
	{
		return; // Stays dirty: runs again once the next world is up.
	}
	bSpeakersDirty = false;

	if (SpeakerWorld.Get() != World)
	{
		DestroySpeakers();
		bSpeakersDirty = false;
		SpeakerWorld = World;
	}

	// Owner speakers by the pawn they belong to, switched off ones included (they still claim the pawn). Channel speakers: UpdateChannelSpeakers.
	TMap<const APawn*, TArray<USteamVoiceComponent*, TInlineAllocator<2>>> PawnSpeakers;
	for (auto It = RegisteredSpeakers.CreateIterator(); It; ++It)
	{
		USteamVoiceComponent* Speaker = It->Get();
		if (!IsValid(Speaker))
		{
			It.RemoveCurrent();
			continue;
		}
		if (Speaker->Source != ESteamVoiceSpeakerSource::Owner || Speaker->GetWorld() != World)
		{
			continue;
		}
		if (const APawn* Pawn = Speaker->GetOwningPawn())
		{
			PawnSpeakers.FindOrAdd(Pawn).Add(Speaker);
		}
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	const bool bAutoAdd = Settings ? Settings->bAutoAddVoiceComponent : true;

	for (TPair<int64, FSpeakerSource>& Pair : SpeakerSources)
	{
		const int64 Talker = Pair.Key;
		FSpeakerSource& Source = Pair.Value;

		const TWeakObjectPtr<APlayerState>* PlayerState = PlayerStates.Find(Talker);
		APawn* Pawn = PlayerState && PlayerState->IsValid() ? (*PlayerState)->GetPawn() : nullptr;
		if (Pawn && (!IsValid(Pawn) || Pawn->GetWorld() != World))
		{
			Pawn = nullptr;
		}
		Source.Pawn = Pawn;

		TArray<USteamVoiceComponent*, TInlineAllocator<2>> Wanted;
		if (Pawn)
		{
			DestroyPluginSpeaker(FallbackSpeakers, Talker);
			const TArray<USteamVoiceComponent*, TInlineAllocator<2>>* OnPawn = PawnSpeakers.Find(Pawn);
			if (OnPawn)
			{
				for (USteamVoiceComponent* Speaker : *OnPawn)
				{
					if (Speaker->IsSpeakerOn())
					{
						Wanted.Add(Speaker);
					}
				}
			}

			// The added speaker belongs to one pawn; after a respawn the new pawn gets its own.
			const TObjectPtr<USteamVoiceComponent>* Auto = AutoSpeakers.Find(Talker);
			USteamVoiceComponent* AutoSpeaker = Auto && IsValid(*Auto) ? Auto->Get() : nullptr;
			if (AutoSpeaker && AutoSpeaker->GetOwningPawn() != Pawn)
			{
				Wanted.Remove(AutoSpeaker);
				DestroyPluginSpeaker(AutoSpeakers, Talker);
				AutoSpeaker = nullptr;
			}

			// The added speaker steps aside as soon as the pawn has one of its own, even a switched off one (walkie off means silent).
			const bool bHasOwnSpeaker = OnPawn && OnPawn->ContainsByPredicate([AutoSpeaker](const USteamVoiceComponent* Speaker) { return Speaker != AutoSpeaker; });
			if (bHasOwnSpeaker || !bAutoAdd)
			{
				if (AutoSpeaker)
				{
					Wanted.Remove(AutoSpeaker);
					DestroyPluginSpeaker(AutoSpeakers, Talker);
				}
			}
			else if (!AutoSpeaker)
			{
				if (USteamVoiceComponent* Added = AddAutoSpeaker(Talker, Pawn))
				{
					Wanted.AddUnique(Added);
				}
			}
			else if (AutoSpeaker->IsSpeakerOn())
			{
				Wanted.AddUnique(AutoSpeaker); // Switched off by the game: stays silent.
			}
		}
		else
		{
			// No pawn (menus, lobby, spectators, dead): one unattached 2D speaker, or silent until a pawn comes back. Fake talkers and the mic test always play.
			DestroyPluginSpeaker(AutoSpeakers, Talker);
			if (PawnlessVoice == ESteamVoicePawnlessMode::Silent && !Source.bFake)
			{
				DestroyPluginSpeaker(FallbackSpeakers, Talker);
			}
			else if (USteamVoiceComponent* Fallback = EnsureFallbackSpeaker(Talker))
			{
				Wanted.Add(Fallback);
			}
		}

		ConnectSpeakers(Talker, Source, Wanted);
	}

	UpdateChannelSpeakers(World);
}

void USteamVoiceSubsystem::UpdateChannelSpeakers(UWorld* World)
{
	// Talker by player state, for the mix without a speaker's owner.
	TMap<const APlayerState*, int64> TalkerByState;
	for (const TPair<int64, TWeakObjectPtr<APlayerState>>& Pair : PlayerStates)
	{
		if (const APlayerState* PlayerState = Pair.Value.Get())
		{
			TalkerByState.Add(PlayerState, Pair.Key);
		}
	}

	TMap<FChannelOutputKey, TArray<USteamVoiceComponent*, TInlineAllocator<4>>> Wanted;
	for (const TWeakObjectPtr<USteamVoiceComponent>& Registered : RegisteredSpeakers)
	{
		USteamVoiceComponent* Speaker = Registered.Get();
		if (!IsValid(Speaker) || Speaker->Source != ESteamVoiceSpeakerSource::Channel || Speaker->GetWorld() != World || !Speaker->IsSpeakerOn())
		{
			continue;
		}

		FChannelOutputKey Key;
		Key.Channel = ResolveChannelId(Speaker->Channel);
		if (Key.Channel < 0)
		{
			continue;
		}

		// The local player's own voice never comes back, so only a remote owner needs a mix without them.
		if (!Speaker->bIncludeOwner)
		{
			const APawn* Pawn = Speaker->GetOwningPawn();
			const int64* Owner = Pawn ? TalkerByState.Find(Pawn->GetPlayerState()) : nullptr;
			Key.Excluded = Owner ? *Owner : 0;
		}
		Wanted.FindOrAdd(Key).Add(Speaker);
	}

	// Disconnect first: a speaker that moved to another mix (new channel, new owner) is cleared before it connects there.
	for (auto It = ChannelOutputs.CreateIterator(); It; ++It)
	{
		const TArray<USteamVoiceComponent*, TInlineAllocator<4>>* Speakers = Wanted.Find(It.Key());
		FChannelOutputState& Output = It.Value();
		for (int32 Index = Output.Connected.Num() - 1; Index >= 0; --Index)
		{
			USteamVoiceComponent* Speaker = Output.Connected[Index].Get();
			// Without a voice source the speaker was switched off or changed channel since: it connects again below.
			if (Speaker && Speaker->HasVoiceSource() && Speakers && Speakers->Contains(Speaker))
			{
				continue;
			}
			if (Speaker)
			{
				Speaker->ClearVoiceSource();
			}
			Output.Connected.RemoveAtSwap(Index);
		}

		if (!Speakers)
		{
			It.RemoveCurrent();
			bChannelOutputsChanged = true;
		}
	}

	const int32 SampleRate = Worker.IsValid() ? Worker->GetDecodeSampleRate() : 0;
	if (SampleRate > 0)
	{
		for (const TPair<FChannelOutputKey, TArray<USteamVoiceComponent*, TInlineAllocator<4>>>& Pair : Wanted)
		{
			FChannelOutputState* Output = ChannelOutputs.Find(Pair.Key);
			if (!Output)
			{
				Output = &ChannelOutputs.Add(Pair.Key);
				Output->Splitter = MakeShared<Audio::FPatchSplitter, ESPMode::ThreadSafe>();
				bChannelOutputsChanged = true;
			}

			for (USteamVoiceComponent* Speaker : Pair.Value)
			{
				if (Output->Connected.Contains(Speaker))
				{
					continue;
				}

				Speaker->SetVoiceSource(FSteamId(), Output->Splitter->AddNewPatch(SampleRate, 1.0f), SampleRate, ChannelSpeakerBufferMs);
				Output->Connected.Add(Speaker);
				if (Output->bActive && !Speaker->IsPlaying())
				{
					Speaker->Start();
				}
			}
		}
	}

	if (bChannelOutputsChanged)
	{
		PublishRouting();
	}
}

void USteamVoiceSubsystem::ConnectSpeakers(int64 Talker, FSpeakerSource& Source, TConstArrayView<USteamVoiceComponent*> Wanted)
{
	// A speaker may have moved to another talker meanwhile (pawn possessed by someone else): only clear it while it still plays this one.
	for (int32 Index = Source.Connected.Num() - 1; Index >= 0; --Index)
	{
		USteamVoiceComponent* Speaker = Source.Connected[Index].Get();
		const bool bStillThisTalker = Speaker && Speaker->GetTalker().Value == Talker;
		if (bStillThisTalker && Wanted.Contains(Speaker))
		{
			continue;
		}
		if (bStillThisTalker)
		{
			Speaker->ClearVoiceSource();
		}
		Source.Connected.RemoveAtSwap(Index);
	}

	if (!Source.Splitter.IsValid() || Source.SampleRate <= 0)
	{
		return;
	}

	// The patch holds up to one second; the speaker keeps it near the playout delay.
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	const int32 PlayoutDelayMs = Settings ? Settings->PlayoutDelayMs : 200;
	for (USteamVoiceComponent* Speaker : Wanted)
	{
		if (!IsValid(Speaker) || Source.Connected.Contains(Speaker))
		{
			continue;
		}

		Speaker->SetVoiceSource(FSteamId(Talker), Source.Splitter->AddNewPatch(Source.SampleRate, 1.0f), Source.SampleRate, PlayoutDelayMs);
		Source.Connected.Add(Speaker);
		if (Source.bActive && !Speaker->IsPlaying())
		{
			Speaker->Start();
		}
	}
}

void USteamVoiceSubsystem::DisconnectSpeakers(int64 Talker, FSpeakerSource& Source)
{
	for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Source.Connected)
	{
		if (Speaker.IsValid() && Speaker->GetTalker().Value == Talker)
		{
			Speaker->ClearVoiceSource();
		}
	}
	Source.Connected.Reset();
}

USteamVoiceComponent* USteamVoiceSubsystem::AddAutoSpeaker(int64 Talker, APawn* Pawn)
{
	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	UClass* Class = Settings ? Settings->VoiceComponentClass.LoadSynchronous() : nullptr;
	if (!Class)
	{
		Class = USteamVoiceComponent::StaticClass();
	}

	USteamVoiceComponent* Speaker = NewObject<USteamVoiceComponent>(Pawn, Class, NAME_None, RF_Transient);
	Speaker->Source = ESteamVoiceSpeakerSource::Owner;
	Speaker->SetUsingAbsoluteRotation(true);
	if (USceneComponent* Root = Pawn->GetRootComponent())
	{
		Speaker->SetupAttachment(Root);
	}
	Speaker->RegisterComponent(); // Begins play on a pawn that already plays, which registers the speaker.
	if (!Speaker->IsSpeakerOn())
	{
		Speaker->Activate(); // The class may have Auto Activate off; an added speaker always starts on.
	}

	AutoSpeakers.Add(Talker, Speaker);
	return Speaker;
}

USteamVoiceComponent* USteamVoiceSubsystem::EnsureFallbackSpeaker(int64 Talker)
{
	if (const TObjectPtr<USteamVoiceComponent>* Existing = FallbackSpeakers.Find(Talker))
	{
		if (IsValid(*Existing))
		{
			return *Existing;
		}
		FallbackSpeakers.Remove(Talker);
	}

	UWorld* World = SpeakerWorld.Get();
	if (!World)
	{
		return nullptr;
	}

	const USteamVoiceSettings* Settings = USteamVoiceSettings::Get();
	UClass* Class = Settings ? Settings->VoiceComponentClass.LoadSynchronous() : nullptr;
	if (!Class)
	{
		Class = USteamVoiceComponent::StaticClass();
	}

	// Unattached, so always 2D: an attenuation would measure the distance to the world origin.
	USteamVoiceComponent* Speaker = NewObject<USteamVoiceComponent>(World, Class, NAME_None, RF_Transient);
	Speaker->Source = ESteamVoiceSpeakerSource::Owner;
	Speaker->bAllowSpatialization = false;
	Speaker->bOverrideAttenuation = false;
	Speaker->AttenuationSettings = nullptr;
	if (!Speaker->SoundClass && Settings && !Settings->VoiceSoundClass.IsNull())
	{
		Speaker->SoundClass = Settings->VoiceSoundClass.LoadSynchronous();
	}
	Speaker->RegisterComponentWithWorld(World);

	FallbackSpeakers.Add(Talker, Speaker);
	return Speaker;
}

void USteamVoiceSubsystem::DestroyPluginSpeaker(TMap<int64, TObjectPtr<USteamVoiceComponent>>& Map, int64 Talker)
{
	TObjectPtr<USteamVoiceComponent> Speaker;
	if (Map.RemoveAndCopyValue(Talker, Speaker) && IsValid(Speaker))
	{
		Speaker->ClearVoiceSource();
		Speaker->DestroyComponent();
	}
}

void USteamVoiceSubsystem::DestroySpeakers()
{
	for (TPair<int64, FSpeakerSource>& Pair : SpeakerSources)
	{
		DisconnectSpeakers(Pair.Key, Pair.Value);
		Pair.Value.Pawn.Reset();
	}

	for (TPair<FChannelOutputKey, FChannelOutputState>& Pair : ChannelOutputs)
	{
		for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Pair.Value.Connected)
		{
			if (Speaker.IsValid())
			{
				Speaker->ClearVoiceSource();
			}
		}
	}
	bChannelOutputsChanged |= !ChannelOutputs.IsEmpty();
	ChannelOutputs.Reset();

	for (TMap<int64, TObjectPtr<USteamVoiceComponent>>* Map : { &AutoSpeakers, &FallbackSpeakers })
	{
		for (const TPair<int64, TObjectPtr<USteamVoiceComponent>>& Pair : *Map)
		{
			if (IsValid(Pair.Value))
			{
				Pair.Value->ClearVoiceSource();
				Pair.Value->DestroyComponent();
			}
		}
		Map->Reset();
	}

	SpeakerWorld.Reset();
	bSpeakersDirty = true;
}

void USteamVoiceSubsystem::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	// Speakers live in the world: release them before it goes, or the world would be kept alive (PIE leak check).
	if (World && World == SpeakerWorld.Get())
	{
		DestroySpeakers();
	}
}

void USteamVoiceSubsystem::TrackPlayerState(int64 Player, APlayerState* PlayerState)
{
	TWeakObjectPtr<APlayerState>& Known = PlayerStates.FindOrAdd(Player);
	if (Known.Get() == PlayerState)
	{
		return;
	}

	if (APlayerState* Old = Known.Get())
	{
		Old->OnPawnSet.RemoveDynamic(this, &USteamVoiceSubsystem::HandlePawnSet);
	}
	Known = PlayerState;
	PlayerState->OnPawnSet.AddUniqueDynamic(this, &USteamVoiceSubsystem::HandlePawnSet);
	bSpeakersDirty = true;
}

void USteamVoiceSubsystem::ForgetPlayerStates()
{
	for (const TPair<int64, TWeakObjectPtr<APlayerState>>& Pair : PlayerStates)
	{
		if (APlayerState* PlayerState = Pair.Value.Get())
		{
			PlayerState->OnPawnSet.RemoveDynamic(this, &USteamVoiceSubsystem::HandlePawnSet);
		}
	}
	PlayerStates.Reset();
}

void USteamVoiceSubsystem::HandlePawnSet(APlayerState* /*Player*/, APawn* /*NewPawn*/, APawn* /*OldPawn*/)
{
	bSpeakersDirty = true;
}

// ---- Push to talk ----

// ---- Server rules ----

bool USteamVoiceSubsystem::IsServerWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	return World && World->GetNetMode() != NM_Client;
}

bool USteamVoiceSubsystem::HasVoiceAuthority() const
{
	return bServerAuthority && IsServerWorld();
}

void USteamVoiceSubsystem::EnsureAuthority(UWorld* World)
{
	if (!bServerAuthority || !World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// One that is already there (the feature started after the world, or started again; on a client the replicated one): use it.
	for (TActorIterator<ASteamVoiceAuthority> It(World); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed())
		{
			HandleAuthorityChanged(*It, false);
			return;
		}
	}
	if (World->GetNetMode() == NM_Client)
	{
		return; // Its authority replicates from the server.
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASteamVoiceAuthority* NewAuthority = World->SpawnActor<ASteamVoiceAuthority>(SpawnParameters);
	if (!NewAuthority)
	{
		UE_LOG(LogSandwichSteam, Error, TEXT("Steam voice: the voice authority could not be spawned in %s; clients wait for voice rules that never come."), *World->GetName());
		return;
	}
	HandleAuthorityChanged(NewAuthority, false);
}

void USteamVoiceSubsystem::HandleAuthorityChanged(ASteamVoiceAuthority* InAuthority, bool bRemoved)
{
	if (!bServerAuthority || !InAuthority)
	{
		return;
	}

	if (bRemoved)
	{
		if (Authority.Get() == InAuthority)
		{
			Authority.Reset();
		}
	}
	else if (Authority.Get() != InAuthority)
	{
		Authority = InAuthority;
		if (InAuthority->HasAuthority())
		{
			// A new authority (new world, travel) starts from what this game instance decided, with a higher version than any before.
			TMap<int64, FSteamVoicePlayerRules> Effective = ServerPlayerRules;
			for (const int64 Player : IdentityConflicts)
			{
				Effective.FindOrAdd(Player).bIdentityConflict = true;
			}
			InAuthority->SetGameRules(bServerVoiceAllowed, ServerPawnlessVoice, ServerOwnerVoiceMaxDistance);
			InAuthority->SetRelayHost(GetServerRelayHost());
			InAuthority->SetChannelMapHash(LocalChannelMapHash);
			InAuthority->ResetPlayerRules(Effective);
			BumpServerRulesVersion();
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: server authority started in %s (rules version %d, %d player rule(s))."), *GetNameSafe(InAuthority->GetWorld()), ServerRulesVersion, ServerPlayerRules.Num());
		}
	}
	ApplyAuthorityRules();
}

bool USteamVoiceSubsystem::CanSetServerRules(FSteamResult& OutResult) const
{
	if (!RequireActive(OutResult))
	{
		return false;
	}
	if (!bServerAuthority)
	{
		OutResult = FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceRulesNeedAuthority", "Server voice rules need Server Authority (Voice settings) and the Steam voice pipeline."));
		return false;
	}
	if (!IsServerWorld())
	{
		OutResult = FSteamResult::Failure(SteamGameplayTags::Error_WrongScope, NSLOCTEXT("SandwichSteam", "VoiceRulesServerOnly", "Only the server sets voice rules."));
		return false;
	}
	return true;
}

FSteamResult USteamVoiceSubsystem::ServerSetPlayerCanTalk(FSteamId Player, bool bCanTalk)
{
	FSteamResult Result;
	if (!CanSetServerRules(Result))
	{
		return Result;
	}
	if (!Player.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceRulesInvalidPlayer", "The Steam ID is not valid."));
	}

	FSteamVoicePlayerRules Rules = ServerPlayerRules.FindRef(Player.Value);
	Rules.bCanTalk = bCanTalk;
	SetServerPlayerRules(Player, Rules);
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::ServerSetPlayerCanUseOwnerVoice(FSteamId Player, bool bCanUse)
{
	FSteamResult Result;
	if (!CanSetServerRules(Result))
	{
		return Result;
	}
	if (!Player.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceRulesInvalidPlayerMouth", "The Steam ID is not valid."));
	}

	FSteamVoicePlayerRules Rules = ServerPlayerRules.FindRef(Player.Value);
	Rules.bCanUseOwnerVoice = bCanUse;
	SetServerPlayerRules(Player, Rules);
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::ServerSetPlayerChannels(FSteamId Player, const FSteamVoiceChannelRule& Transmit, const FSteamVoiceChannelRule& Listen)
{
	FSteamResult Result;
	if (!CanSetServerRules(Result))
	{
		return Result;
	}
	if (!Player.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceRulesInvalidPlayerChannels", "The Steam ID is not valid."));
	}

	FSteamVoicePlayerRules Rules = ServerPlayerRules.FindRef(Player.Value);
	if (!NormalizeChannelRule(Transmit, Rules.Transmit, Result) || !NormalizeChannelRule(Listen, Rules.Listen, Result))
	{
		return Result;
	}
	SetServerPlayerRules(Player, Rules);
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::ServerSetOwnerVoiceMaxDistance(float Distance)
{
	FSteamResult Result;
	if (!CanSetServerRules(Result))
	{
		return Result;
	}
	if (!(Distance >= 0.0f))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoiceDistanceInvalid", "The owner voice max distance must be 0 (no limit) or more."));
	}

	if (Distance != ServerOwnerVoiceMaxDistance)
	{
		ServerOwnerVoiceMaxDistance = Distance;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: the server limits owner voice to %s."), Distance > 0.0f ? *FString::Printf(TEXT("%.0f cm"), Distance) : TEXT("no distance (off)"));
		PushServerGameRules();
	}
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::ServerSetPlayerOwnerVoiceMaxDistance(FSteamId Player, float Distance)
{
	FSteamResult Result;
	if (!CanSetServerRules(Result))
	{
		return Result;
	}
	if (!Player.IsValid() || FMath::IsNaN(Distance))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "VoicePlayerDistanceInvalid", "The Steam ID or the distance is not valid."));
	}

	FSteamVoicePlayerRules Rules = ServerPlayerRules.FindRef(Player.Value);
	Rules.OwnerVoiceMaxDistance = Distance < 0.0f ? -1.0f : Distance;
	SetServerPlayerRules(Player, Rules);
	return FSteamResult::Success();
}

FSteamResult USteamVoiceSubsystem::ServerSetVoiceTopology(ESteamVoiceTopology NewTopology)
{
	FSteamResult Result;
	if (!CanSetServerRules(Result))
	{
		return Result;
	}

	if (NewTopology != ServerTopology)
	{
		ServerTopology = NewTopology;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: the server switches voice to %s."),
			NewTopology == ESteamVoiceTopology::ServerRelay ? TEXT("Server Relay (clients send to this host, it forwards)") : TEXT("peer to peer"));
		PushServerGameRules();
	}
	if (NewTopology == ESteamVoiceTopology::ServerRelay && !LocalId.IsValid())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: Server Relay needs this host's Steam ID; voice stays peer to peer until Steam has signed in."));
	}
	return FSteamResult::Success();
}

FSteamId USteamVoiceSubsystem::GetServerRelayHost() const
{
	return ServerTopology == ESteamVoiceTopology::ServerRelay && LocalId.IsValid() ? LocalId : FSteamId();
}

bool USteamVoiceSubsystem::IsVoiceRelayHost() const
{
	return AppliedRelayHost != 0 && LocalId.IsValid() && AppliedRelayHost == LocalId.Value && HasVoiceAuthority();
}

void USteamVoiceSubsystem::SetRelayFilter(TSharedPtr<ISteamVoiceRelayFilter, ESPMode::ThreadSafe> Filter)
{
	check(IsInGameThread());
	if (Filter == RelayFilter)
	{
		return;
	}

	// The worker gets it with the next snapshot and holds a reference while it calls it, so the old one stays alive until it is done.
	RelayFilter = MoveTemp(Filter);
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: relay filter %s."), RelayFilter.IsValid() ? TEXT("set") : TEXT("removed"));
	bRoutingPublished = false;
	PublishRouting();
}

FSteamVoicePlayerRules USteamVoiceSubsystem::GetPlayerVoiceRules(FSteamId Player) const
{
	// The server's own map is current even between worlds, when no authority actor exists.
	return HasVoiceAuthority() ? GetEffectiveServerRules(Player.Value) : AppliedPlayerRules.FindRef(Player.Value);
}

FSteamVoicePlayerRules USteamVoiceSubsystem::GetEffectiveServerRules(int64 Player) const
{
	FSteamVoicePlayerRules Rules = ServerPlayerRules.FindRef(Player);
	Rules.bIdentityConflict = IdentityConflicts.Contains(Player);
	return Rules;
}

void USteamVoiceSubsystem::PushServerPlayerRules(int64 Player)
{
	if (ASteamVoiceAuthority* Actor = Authority.Get(); Actor && Actor->HasAuthority())
	{
		Actor->SetPlayerRules(FSteamId(Player), GetEffectiveServerRules(Player));
		BumpServerRulesVersion();
	}
	ApplyAuthorityRules();
}

void USteamVoiceSubsystem::UpdateIdentityConflicts(TConstArrayView<int64> Ids, TConstArrayView<APlayerState*> States)
{
	TSet<int64> Duplicates;
	SandwichSteam::Voice::FindDuplicateIds(Ids, Duplicates);
	if (Duplicates.Num() == IdentityConflicts.Num() && Duplicates.Includes(IdentityConflicts))
	{
		return;
	}

	TArray<int64> Changed;
	for (const int64 Player : Duplicates)
	{
		if (!IdentityConflicts.Contains(Player))
		{
			Changed.Add(Player);
		}
	}
	for (const int64 Player : IdentityConflicts)
	{
		if (!Duplicates.Contains(Player))
		{
			Changed.Add(Player);
		}
	}
	IdentityConflicts = MoveTemp(Duplicates);

	for (const int64 Player : Changed)
	{
		PushServerPlayerRules(Player);
		if (!IdentityConflicts.Contains(Player))
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: only one player has Steam ID %s again; their voice follows the rules again."), *FSteamId(Player).ToString());
			continue;
		}

		// The player array grows at the end, so the last player state with the id is the one that joined.
		APlayerState* Joining = nullptr;
		for (int32 Index = Ids.Num() - 1; Index >= 0; --Index)
		{
			if (Ids[Index] == Player)
			{
				Joining = States[Index];
				break;
			}
		}
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: two players have Steam ID %s (joining: %s). Their voice is refused until one leaves; On Voice Identity Conflict lets the game kick. Steam auth verifies Steam IDs."),
			*FSteamId(Player).ToString(), *GetNameSafe(Joining));
		OnVoiceIdentityConflict.Broadcast(FSteamId(Player), Joining);
	}
}

void USteamVoiceSubsystem::UpdateOwnerVoiceRange()
{
	MouthNotSent.Reset();
	MouthDropped.Reset();
	RelayMouthOutOfRange.Reset();

	bool bAnyLimit = OwnerVoiceMaxDistance > 0.0f;
	for (const TPair<int64, FSteamVoicePlayerRules>& Pair : AppliedPlayerRules)
	{
		bAnyLimit |= Pair.Value.OwnerVoiceMaxDistance > 0.0f;
	}
	// Server Relay clients send everything to the host, which decides every distance with the server's pawns (a pawn that is not net relevant
	// here would wrongly drop voice).
	const bool bRelayHost = IsVoiceRelayHost();
	if (!bAnyLimit || (AppliedRelayHost != 0 && !bRelayHost))
	{
		MouthInRange.Reset();
		RelayMouthInRange.Reset();
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(GameInstance->GetWorld()) : nullptr;
	const APawn* LocalPawn = Controller ? Controller->GetPawn() : nullptr;
	const FSteamVoicePlayerRules* LocalRules = LocalId.IsValid() ? AppliedPlayerRules.Find(LocalId.Value) : nullptr;
	const float LocalDistance = SandwichSteam::Voice::ResolveOwnerVoiceDistance(OwnerVoiceMaxDistance, LocalRules ? LocalRules->OwnerVoiceMaxDistance : 0.0f);

	for (auto It = MouthInRange.CreateIterator(); It; ++It)
	{
		if (!Present.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	// Distance between our pawn and theirs, as this client sees them. A pawn that is not net relevant here is gone, so it counts as out of range.
	for (const int64 Player : Present)
	{
		if (Player == LocalId.Value || SandwichSteam::Voice::IsFakeTalkerId(static_cast<uint64>(Player)))
		{
			continue; // Fake talkers have no pawn: they are test tools and keep their mouth.
		}

		const TWeakObjectPtr<APlayerState>* State = PlayerStates.Find(Player);
		const APawn* Pawn = State && State->IsValid() ? (*State)->GetPawn() : nullptr;
		const bool bKnown = IsValid(LocalPawn) && IsValid(Pawn);
		const double DistanceSquared = bKnown ? FVector::DistSquared(LocalPawn->GetActorLocation(), Pawn->GetActorLocation()) : 0.0;

		// Sending: our distance, with hysteresis. Receiving: the talker's distance, with a wider margin.
		bool& bInRange = MouthInRange.FindOrAdd(Player, false);
		bInRange = SandwichSteam::Voice::UpdateMouthInRange(bInRange, bKnown, DistanceSquared, LocalDistance);
		if (!bInRange)
		{
			MouthNotSent.Add(static_cast<uint64>(Player));
		}
		const FSteamVoicePlayerRules* Rules = AppliedPlayerRules.Find(Player);
		const float TalkerDistance = SandwichSteam::Voice::ResolveOwnerVoiceDistance(OwnerVoiceMaxDistance, Rules ? Rules->OwnerVoiceMaxDistance : 0.0f);
		if (!SandwichSteam::Voice::IsMouthAcceptable(bKnown, DistanceSquared, TalkerDistance))
		{
			MouthDropped.Add(static_cast<uint64>(Player));
		}
	}
	MouthNotSent.Sort();
	MouthDropped.Sort();

	if (!bRelayHost)
	{
		RelayMouthInRange.Reset();
		return;
	}

	// Relay host: every talker -> recipient pair between the other players, with the server's pawns.
	const auto FindPawn = [this](int64 Player) -> const APawn*
	{
		const TWeakObjectPtr<APlayerState>* State = PlayerStates.Find(Player);
		return State && State->IsValid() ? (*State)->GetPawn() : nullptr;
	};
	const auto IsRelayed = [this](int64 Player)
	{
		return Player != LocalId.Value && Present.Contains(Player) && !SandwichSteam::Voice::IsFakeTalkerId(static_cast<uint64>(Player));
	};

	for (auto It = RelayMouthInRange.CreateIterator(); It; ++It)
	{
		if (!IsRelayed(It.Key().Key) || !IsRelayed(It.Key().Value))
		{
			It.RemoveCurrent();
		}
	}

	for (const int64 Talker : Present)
	{
		if (!IsRelayed(Talker))
		{
			continue;
		}
		const FSteamVoicePlayerRules* Rules = AppliedPlayerRules.Find(Talker);
		const float TalkerDistance = SandwichSteam::Voice::ResolveOwnerVoiceDistance(OwnerVoiceMaxDistance, Rules ? Rules->OwnerVoiceMaxDistance : 0.0f);
		if (TalkerDistance <= 0.0f)
		{
			continue; // Their mouth carries everywhere.
		}

		const APawn* TalkerPawn = FindPawn(Talker);
		for (const int64 Recipient : Present)
		{
			if (Recipient == Talker || !IsRelayed(Recipient))
			{
				continue;
			}

			const APawn* RecipientPawn = FindPawn(Recipient);
			const bool bKnown = IsValid(TalkerPawn) && IsValid(RecipientPawn);
			const double DistanceSquared = bKnown ? FVector::DistSquared(TalkerPawn->GetActorLocation(), RecipientPawn->GetActorLocation()) : 0.0;
			bool& bInRange = RelayMouthInRange.FindOrAdd(TPair<int64, int64>(Talker, Recipient), false);
			bInRange = SandwichSteam::Voice::UpdateMouthInRange(bInRange, bKnown, DistanceSquared, TalkerDistance);
			if (!bInRange)
			{
				RelayMouthOutOfRange.FindOrAdd(static_cast<uint64>(Talker)).Add(static_cast<uint64>(Recipient));
			}
		}
	}
	for (TPair<uint64, TArray<uint64>>& Pair : RelayMouthOutOfRange)
	{
		Pair.Value.Sort();
	}
}

void USteamVoiceSubsystem::ResolveAppliedRules()
{
	// A tag this build cannot map allows nothing (fail closed); the channel map hash warns about it.
	const TMap<FGameplayTag, int32>& TagMap = USteamVoiceSettings::Get()->ChannelTagMap;
	const auto ResolveChannels = [&TagMap](const FSteamVoiceChannelRule& In, SandwichSteam::Voice::FChannelRule& Out)
	{
		Out.bRestricted = In.bRestricted;
		Out.Allowed.Reset();
		if (In.bRestricted)
		{
			for (const FSteamVoiceChannel& Channel : In.Channels)
			{
				const int32 Id = Channel.Resolve(TagMap);
				if (Id >= 0)
				{
					Out.Allowed.AddUnique(Id);
				}
			}
			Out.Allowed.Sort();
		}
	};

	ResolvedRules.Reset();
	ResolvedRules.Reserve(AppliedPlayerRules.Num());
	for (const TPair<int64, FSteamVoicePlayerRules>& Pair : AppliedPlayerRules)
	{
		SandwichSteam::Voice::FPlayerRules& Rules = ResolvedRules.Add(Pair.Key);
		Rules.bCanTalk = Pair.Value.bCanTalk;
		Rules.bOwnerVoice = Pair.Value.bCanUseOwnerVoice;
		Rules.bRefused = Pair.Value.bIdentityConflict;
		ResolveChannels(Pair.Value.Transmit, Rules.Transmit);
		ResolveChannels(Pair.Value.Listen, Rules.Listen);
	}
}

void USteamVoiceSubsystem::SetServerPlayerRules(FSteamId Player, const FSteamVoicePlayerRules& Rules)
{
	const FSteamVoicePlayerRules* Current = ServerPlayerRules.Find(Player.Value);
	if (Current ? *Current == Rules : Rules.IsDefault())
	{
		return;
	}

	if (Rules.IsDefault())
	{
		ServerPlayerRules.Remove(Player.Value);
	}
	else
	{
		ServerPlayerRules.Add(Player.Value, Rules);
	}
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: server rules for %s: %s."), *Player.ToString(), *DescribeRules(Rules));
	PushServerPlayerRules(Player.Value);
}

void USteamVoiceSubsystem::PushServerGameRules()
{
	if (ASteamVoiceAuthority* Actor = Authority.Get(); Actor && Actor->HasAuthority())
	{
		Actor->SetGameRules(bServerVoiceAllowed, ServerPawnlessVoice, ServerOwnerVoiceMaxDistance);
		Actor->SetRelayHost(GetServerRelayHost());
		BumpServerRulesVersion();
	}
	ApplyAuthorityRules();
}

void USteamVoiceSubsystem::BumpServerRulesVersion()
{
	++ServerRulesVersion;
	if (ASteamVoiceAuthority* Actor = Authority.Get(); Actor && Actor->HasAuthority())
	{
		Actor->SetRulesVersion(ServerRulesVersion);
	}
}

void USteamVoiceSubsystem::ApplyAuthorityRules()
{
	if (!bServerAuthority)
	{
		return;
	}

	// No authority (joining, travel) or one the server has not filled: wait, and forget the rules of the last server.
	const ASteamVoiceAuthority* Actor = Authority.Get();
	const bool bReceived = Actor && Actor->GetRulesVersion() > 0;
	bool bNewAllowed = bVoiceAllowed;
	ESteamVoicePawnlessMode NewPawnless = PawnlessVoice;
	float NewDistance = OwnerVoiceMaxDistance;
	bool bNewMismatch = false;
	TMap<int64, FSteamVoicePlayerRules> NewRules;
	if (bReceived)
	{
		bNewAllowed = Actor->IsVoiceAllowed();
		NewPawnless = Actor->GetPawnlessVoice();
		NewDistance = FMath::Max(Actor->GetOwnerVoiceMaxDistance(), 0.0f);
		bNewMismatch = Actor->GetChannelMapHash() != LocalChannelMapHash;
		for (const FSteamVoicePlayerRulesItem& Item : Actor->GetPlayerRules())
		{
			if (Item.Player.IsValid() && !Item.Rules.IsDefault())
			{
				NewRules.Add(Item.Player.Value, Item.Rules);
			}
		}
	}
	const int32 NewVersion = bReceived ? Actor->GetRulesVersion() : 0;
	const int64 NewRelayHost = bReceived ? Actor->GetRelayHost().Value : 0;

	if (bReceived == bRulesReceived && NewVersion == AppliedRulesVersion && bNewAllowed == bVoiceAllowed && NewPawnless == PawnlessVoice
		&& NewDistance == OwnerVoiceMaxDistance && bNewMismatch == bChannelMapMismatch && NewRelayHost == AppliedRelayHost && NewRules.OrderIndependentCompareEqual(AppliedPlayerRules))
	{
		return;
	}

	const bool bReceivedChanged = bReceived != bRulesReceived;
	const bool bAllowedChanged = bNewAllowed != bVoiceAllowed;
	if (bNewMismatch && !bChannelMapMismatch)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam voice: the server's Channel Tag Map differs from this build's. Channel rules and Channel speakers may not match the server's channels; give every build the same map (Project Settings > Plugins > Sandwich Steam - Voice)."));
	}
	bChannelMapMismatch = bNewMismatch;
	bRulesReceived = bReceived;
	AppliedRulesVersion = NewVersion;
	AppliedPlayerRules = MoveTemp(NewRules);
	OwnerVoiceMaxDistance = NewDistance;
	bVoiceAllowed = bNewAllowed;
	const bool bWasRelayHost = IsVoiceRelayHost();
	if (NewRelayHost != AppliedRelayHost)
	{
		AppliedRelayHost = NewRelayHost;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: voice runs %s."), AppliedRelayHost == 0 ? TEXT("peer to peer")
			: IsVoiceRelayHost() ? TEXT("through this host (Server Relay)") : *FString::Printf(TEXT("through the host %s (Server Relay)"), *FSteamId(AppliedRelayHost).ToString()));
	}
	ResolveAppliedRules();
	if (NewPawnless != PawnlessVoice)
	{
		PawnlessVoice = NewPawnless;
		bSpeakersDirty = true;
	}

	RefreshTalking(false);
	UpdateOwnerVoiceRange();
	bRoutingPublished = false;
	PublishRouting();

	// A relay host whose player has voice off still runs the pipeline for the others; it stops once it no longer relays.
	if (!bEnabled && bWasRelayHost != IsVoiceRelayHost())
	{
		ApplyEnabledState();
	}

	if (bReceivedChanged)
	{
		if (bRulesReceived)
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: server voice rules received (version %d): voice %s, players without a pawn %s, %d player rule(s)."), AppliedRulesVersion,
				bVoiceAllowed ? TEXT("allowed") : TEXT("not allowed"), PawnlessVoice == ESteamVoicePawnlessMode::Silent ? TEXT("silent") : TEXT("2D"), AppliedPlayerRules.Num());
		}
		else
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam voice: waiting for the server's voice rules; nothing is sent or played until they arrive."));
		}
	}
	else
	{
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam voice: server voice rules version %d applied."), AppliedRulesVersion);
	}

	if (bAllowedChanged)
	{
		OnVoiceAllowedChanged.Broadcast(bVoiceAllowed);
	}
	OnVoiceRulesChanged.Broadcast();
}

bool USteamVoiceSubsystem::CanLocalTalkByRules() const
{
	const FSteamVoicePlayerRules* Rules = LocalId.IsValid() ? AppliedPlayerRules.Find(LocalId.Value) : nullptr;
	return !Rules || Rules->MayTalk();
}

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
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "VoiceFakeNeedsPipeline", "Fake talkers need the Steam voice pipeline (turn on Use Steam Voice Pipeline in the Voice settings, run Configure Steam and restart)."));
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

FSteamResult USteamVoiceSubsystem::SetRelayEcho(bool bEcho)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}
	if (bEcho && !IsVoiceRelayHost())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_WrongScope, NSLOCTEXT("SandwichSteam", "VoiceRelayEchoHostOnly", "Relay echo runs on the relay host: Server Authority on, Topology Server Relay, on the server."));
	}

	bRelayEcho = bEcho;
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

	Report += FString::Printf(TEXT("  Voice: %s, %s, mode %s, microphone %s (requested %s), mic test %s, headset %s\n"),
		bEnabled ? TEXT("enabled") : TEXT("disabled"), bVoiceAllowed ? TEXT("allowed") : TEXT("not allowed by the game (nothing sent or played)"), Mode == ESteamVoiceMode::PushToTalk ? TEXT("push to talk") : TEXT("open mic"),
		bMicOpen ? TEXT("open") : TEXT("closed"), bTalkRequested ? TEXT("yes") : TEXT("no"), bMicTest ? TEXT("on (plays back, sends nothing by itself)") : TEXT("off"), IsHeadsetPresent() ? TEXT("present") : TEXT("not found"));
	Report += FString::Printf(TEXT("  Local user: %s, local talking: %s\n"), *LocalId.ToString(), IsLocalTalking() ? TEXT("yes") : TEXT("no"));
	if (bServerAuthority)
	{
		const FString Rules = !bRulesReceived ? FString(TEXT("WAITING FOR RULES (nothing sent or played)"))
			: FString::Printf(TEXT("rules version %d, voice %s, pawnless %s, owner voice distance %s, %d player rule(s)%s"), AppliedRulesVersion, bVoiceAllowed ? TEXT("allowed") : TEXT("not allowed"),
				PawnlessVoice == ESteamVoicePawnlessMode::Silent ? TEXT("silent") : TEXT("2D"), OwnerVoiceMaxDistance > 0.0f ? *FString::Printf(TEXT("%.0f cm"), OwnerVoiceMaxDistance) : TEXT("off"),
				AppliedPlayerRules.Num(), bChannelMapMismatch ? TEXT(", CHANNEL MAP DIFFERS FROM THE SERVER'S") : TEXT(""));
		Report += FString::Printf(TEXT("  Server authority: on, %s, %s\n"), HasVoiceAuthority() ? TEXT("this game instance is the server") : TEXT("client"), *Rules);
		for (const TPair<int64, FSteamVoicePlayerRules>& Pair : AppliedPlayerRules)
		{
			Report += FString::Printf(TEXT("    rules: %s%s: %s\n"), *FSteamId(Pair.Key).ToString(), Pair.Key == LocalId.Value ? TEXT(" (you)") : TEXT(""), *DescribeRules(Pair.Value));
		}
		if (!MouthNotSent.IsEmpty() || !MouthDropped.IsEmpty())
		{
			Report += FString::Printf(TEXT("    owner voice out of range: %d player(s) not sent our voice, %d player(s) whose voice is dropped here\n"), MouthNotSent.Num(), MouthDropped.Num());
		}
		const FString Topology = AppliedRelayHost == 0 ? FString(TEXT("peer to peer"))
			: IsVoiceRelayHost() ? FString::Printf(TEXT("Server Relay, this host forwards%s, relay filter %s, echo %s"), bEnabled ? TEXT("") : TEXT(" (its own voice is off: relay only)"),
				RelayFilter.IsValid() ? TEXT("set") : TEXT("none"), bRelayEcho ? TEXT("on") : TEXT("off"))
			: FString::Printf(TEXT("Server Relay through %s"), *FSteamId(AppliedRelayHost).ToString());
		Report += FString::Printf(TEXT("    topology: %s (server start value %s)\n"), *Topology,
			ServerTopology == ESteamVoiceTopology::ServerRelay ? TEXT("Server Relay") : TEXT("peer to peer"));
		if (IsVoiceRelayHost())
		{
			int32 OutOfRangePairs = 0;
			for (const TPair<uint64, TArray<uint64>>& Pair : RelayMouthOutOfRange)
			{
				OutOfRangePairs += Pair.Value.Num();
			}
			Report += FString::Printf(TEXT("    relay owner voice: %d talker -> listener pair(s) out of range\n"), OutOfRangePairs);
		}
	}
	else
	{
		Report += TEXT("  Server authority: off (every client receives every player; proximity and channels are not private)\n");
	}
	{
		FString Channels;
		for (const FSteamVoiceChannel& Channel : TransmitChannels)
		{
			const int32 Id = ResolveChannelId(Channel);
			const FString Name = Channel.Tag.IsValid() ? Channel.Tag.ToString() : FString::FromInt(Channel.Id);
			Channels += Channels.IsEmpty() ? TEXT("") : TEXT(", ");
			Channels += Id < 0 ? FString::Printf(TEXT("%s (unmapped, not sent)"), *Name)
				: (Channel.Tag.IsValid() ? FString::Printf(TEXT("%s (%d)"), *Name, Id) : Name);
		}
		Report += FString::Printf(TEXT("  Transmit: mouth %s, channels %s (after the server rules: mouth %s, %d channel(s))\n"), bTransmitToOwner ? TEXT("on") : TEXT("off"),
			Channels.IsEmpty() ? TEXT("none") : *Channels, bPublishedTransmitMouth ? TEXT("on") : TEXT("off"), PublishedTransmitChannels.Num());
	}

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
			FString Where;
			if (bUseSteamPipeline)
			{
				const FPlayerTransmit* Transmit = PlayerTransmits.Find(Player);
				FString Channels;
				for (const FSteamVoiceChannel& Channel : Transmit ? MakeChannels(Transmit->Channels) : TArray<FSteamVoiceChannel>())
				{
					Channels += Channels.IsEmpty() ? TEXT("") : TEXT(", ");
					Channels += Channel.Tag.IsValid() ? Channel.Tag.ToString() : FString::FromInt(Channel.Id);
				}
				Where = FString::Printf(TEXT(", mouth %s, channels %s%s"), Transmit && Transmit->bMouth ? TEXT("on") : TEXT("off"), Channels.IsEmpty() ? TEXT("none") : *Channels,
					Book.IsValid() && Book->IsMuted(Player) ? TEXT(" (muted, reported)") : TEXT(""));
			}
			Report += FString::Printf(TEXT("    talking: %s%s%s\n"), *FSteamId(Player).ToString(), Player == LocalId.Value ? TEXT(" (you)") : TEXT(""), *Where);
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

	Report += FString::Printf(TEXT("  Player volumes: %d set\n"), PlayerVolumes.Num());
	for (const TPair<int64, float>& Pair : PlayerVolumes)
	{
		Report += FString::Printf(TEXT("    volume: %s %.2f%s\n"), *FSteamId(Pair.Key).ToString(), Pair.Value, Present.Contains(Pair.Key) ? TEXT("") : TEXT(" (not in the game)"));
	}

	Report += FString::Printf(TEXT("  Counters: %d talking events, %d mute call(s) not accepted yet\n"), TalkingEventsSent, MuteCallsFailed);

	Report += FString::Printf(TEXT("  Pipeline: %s\n"), bUseSteamPipeline ? (Worker.IsValid() ? TEXT("Steam voice (running)") : TEXT("Steam voice (stopped)")) : TEXT("Online Subsystem voice"));
	if (Worker.IsValid())
	{
		const SandwichSteam::Voice::FWorkerStats Stats = Worker->GetStats();
		Report += FString::Printf(TEXT("    Worker: %s, loop avg %.2f ms / max %.2f ms, recording %s, last capture result %d\n"),
			Stats.bRunning ? TEXT("running") : TEXT("stopped"), Stats.LoopAverageMs, Stats.LoopMaxMs, Stats.bRecording ? TEXT("yes") : TEXT("no"), Stats.LastCaptureResult);
		Report += FString::Printf(TEXT("    Sample rate: Steam optimal %d Hz, decoding at %d Hz\n"), Stats.OptimalSampleRate, Stats.DecodeSampleRate);
		Report += FString::Printf(TEXT("    Capture: %llu chunk(s), %llu bytes, %llu too large, %llu not transmitted (no mouth, no channel); sent %llu packet(s), %llu failed, %llu skipped for a peer (server rules), %llu hello(s)\n"),
			Stats.CapturedChunks, Stats.CapturedBytes, Stats.CaptureTooLarge, Stats.NotTransmitted, Stats.PacketsSent, Stats.SendFailures, Stats.RuleNotSent, Stats.HellosSent);
		Report += FString::Printf(TEXT("    Microphone: level %.3f, voice activation %s, %llu packet(s) below it not sent, %llu could not be measured\n"),
			Worker->GetLocalLevel(), Stats.ActivationThreshold > 0.0f ? *FString::Printf(TEXT("%.3f"), Stats.ActivationThreshold) : TEXT("off"), Stats.GatedChunks, Stats.LevelDecodeErrors);
		Report += FString::Printf(TEXT("    Payload format: %llu/%llu start with our Steam ID, %llu/%llu end with a valid CRC-32 -> %s\n"),
			Stats.PayloadIdMatches, Stats.CapturedChunks, Stats.PayloadCrcMatches, Stats.CapturedChunks,
			Stats.CapturedChunks == 0 ? TEXT("no capture yet")
			: (Stats.PayloadIdMatches == Stats.CapturedChunks && Stats.PayloadCrcMatches == Stats.CapturedChunks ? TEXT("CONFIRMED") : TEXT("NOT as expected")));
		if (FakeTalkerCount > 0)
		{
			Report += FString::Printf(TEXT("    Fake speaker ids: %llu packet(s) rewritten, %llu kept ours (layout not as expected)\n"), Stats.FakeRewritten, Stats.FakeRewriteFailed);
		}
		Report += FString::Printf(TEXT("    Receive: %llu packet(s), %llu invalid, %llu from non-players, %llu muted, %llu dropped by the server rules, %llu mouth removed and %llu with channels removed by the server rules, %llu simulated loss, %llu dropped: speaker id is not the sender\n"),
			Stats.PacketsReceived, Stats.Invalid, Stats.FromUnknown, Stats.MutedDrops, Stats.RuleDrops, Stats.RuleMouthRemoved, Stats.RuleChannelsRemoved, Stats.SimLossDrops, Stats.SpeakerMismatch);
		Report += FString::Printf(TEXT("    Sessions: %llu accepted, %llu rejected, %d pending\n"), Stats.SessionsAccepted, Stats.SessionsRejected, Stats.SessionsPending);
		if (IsVoiceRelayHost())
		{
			Report += FString::Printf(TEXT("    Relay: %llu packet(s) in, %llu send(s) forwarded, %llu dropped by the talker's rules, %llu by the filter, %llu recipient(s) left nothing, %llu echo send(s), upload %.1f KB/s\n"),
				Stats.RelayIn, Stats.RelayForwarded, Stats.RelayRuleDrops, Stats.RelayFilterDrops, Stats.RelayNotSent, Stats.RelayEchoSent, Stats.RelayUploadKBps);
		}
		else if (AppliedRelayHost != 0)
		{
			Report += FString::Printf(TEXT("    Relay: client, sending to %s only; %llu relayed packet(s) for talkers not in the game\n"), *FSteamId(AppliedRelayHost).ToString(), Stats.RelayUnknownTalker);
		}
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

		Report += FString::Printf(TEXT("    Speakers: %d registered, %d added to pawns, %d without a pawn (2D); pawnless voice %s\n"), RegisteredSpeakers.Num(), AutoSpeakers.Num(), FallbackSpeakers.Num(),
			PawnlessVoice == ESteamVoicePawnlessMode::Silent ? TEXT("silent") : TEXT("2D"));
		Report += FString::Printf(TEXT("    Channels: %d mix(es), %llu voice packet(s) not decoded (no mouth and no channel played here)\n"), Stats.Channels.Num(), Stats.NotPlayed);
		for (const SandwichSteam::Voice::FChannelOutputStats& Channel : Stats.Channels)
		{
			const FChannelOutputState* Output = ChannelOutputs.Find(FChannelOutputKey{ Channel.Channel, static_cast<int64>(Channel.Excluded) });
			const USteamVoiceComponent* Component = nullptr;
			int32 NumSpeakers = 0;
			if (Output)
			{
				for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Output->Connected)
				{
					if (const USteamVoiceComponent* Connected = Speaker.Get())
					{
						Component = Component ? Component : Connected;
						++NumSpeakers;
					}
				}
			}

			Report += FString::Printf(TEXT("      channel %d%s: %s%s, %d talker(s), gain %.2f, limited %llu sample(s), overflow %llu sample(s); speakers: %d, first %s, buffered %.0f ms, underruns %u\n"),
				Channel.Channel, Channel.Excluded != 0 ? *FString::Printf(TEXT(" minus %s"), *FSteamId(static_cast<int64>(Channel.Excluded)).ToString()) : TEXT(""),
				Channel.bRunning ? TEXT("mixing") : TEXT("idle"), Channel.bActive ? TEXT(", speakers on") : TEXT(""), Channel.ActiveTalkers, Channel.Gain,
				Channel.Limited, Channel.OverflowSamples, NumSpeakers, Component ? (Component->IsPlaying() ? TEXT("playing") : TEXT("stopped")) : TEXT("none"),
				Component ? Component->GetBufferedMs() : 0.0f, Component ? Component->GetUnderruns() : 0u);
		}
		for (const SandwichSteam::Voice::FTalkerStats& Talker : Stats.Talkers)
		{
			const FSpeakerSource* Source = SpeakerSources.Find(static_cast<int64>(Talker.Talker));
			const USteamVoiceComponent* Component = nullptr;
			int32 NumSpeakers = 0;
			if (Source)
			{
				for (const TWeakObjectPtr<USteamVoiceComponent>& Speaker : Source->Connected)
				{
					if (const USteamVoiceComponent* Connected = Speaker.Get())
					{
						Component = Component ? Component : Connected;
						++NumSpeakers;
					}
				}
			}
			const TObjectPtr<USteamVoiceComponent>* Fallback = FallbackSpeakers.Find(static_cast<int64>(Talker.Talker));
			const TObjectPtr<USteamVoiceComponent>* Auto = AutoSpeakers.Find(static_cast<int64>(Talker.Talker));
			const FString Placement = Fallback && IsValid(*Fallback) ? FString(TEXT("no pawn, 2D"))
				: Source && Source->Pawn.IsValid() ? FString::Printf(TEXT("pawn %s%s"), *Source->Pawn->GetName(), Auto && IsValid(*Auto) ? TEXT(", added") : TEXT(""))
				: Source && !Source->bFake && PawnlessVoice == ESteamVoicePawnlessMode::Silent ? FString(TEXT("no pawn, silent"))
				: FString(TEXT("not placed"));
			const float BufferedMs = Component ? Component->GetBufferedMs() : 0.0f;

			FString Latency = TEXT("n/a");
			if (Talker.PipelineLatencyMs >= 0.0f)
			{
				Latency = FString::Printf(TEXT("%.0f ms (capture to speaker %.0f + buffer %.0f)"), Talker.PipelineLatencyMs + BufferedMs, Talker.PipelineLatencyMs, BufferedMs);
			}

			Report += FString::Printf(TEXT("    talker %s%s: %s%s, level %.3f, volume %.2f, packets %llu, lost %llu, late %llu, dup %llu, resync %llu, flood drops %llu%s, decode errors %llu (last result %d, last %d samples, longest %.0f ms), pause silence dropped %.0f ms, queued %d\n"),
				*FSteamId(static_cast<int64>(Talker.Talker)).ToString(), Talker.Talker == SandwichSteam::Voice::MicTestTalkerId ? TEXT(" (mic test)")
					: Talker.Talker == SandwichSteam::Voice::RelayEchoTalkerId ? TEXT(" (relay echo)") : Talker.bFake ? TEXT(" (fake)") : TEXT(""),
				Talker.bTalking ? TEXT("TALKING") : TEXT("silent"), Talker.bActive ? TEXT(", speaker on") : TEXT(""),
				Talker.Level, Talker.Gain, Talker.Packets, Talker.Lost, Talker.Late, Talker.Duplicate, Talker.Resyncs, Talker.FloodDrops,
				Talker.bFlooded ? TEXT(" (MUTED: flood)") : TEXT(""), Talker.DecodeErrors, Talker.LastDecodeResult, Talker.LastDecodedSamples, Talker.MaxPacketMs, Talker.SilenceTrimmedMs, Talker.Queued);
			Report += FString::Printf(TEXT("      speakers: %d (%s), first %s, buffered %.0f ms, underruns %u (incl. ends of speech), latency %s\n"),
				NumSpeakers, *Placement, Component ? (Component->IsPlaying() ? TEXT("playing") : TEXT("stopped")) : TEXT("none"), BufferedMs,
				Component ? Component->GetUnderruns() : 0u, *Latency);
			if (IsVoiceRelayHost() && !Talker.bFake)
			{
				Report += FString::Printf(TEXT("      relay: %llu send(s) forwarded, %llu packet(s) dropped (rules, filter)\n"), Talker.RelayForwarded, Talker.RelayDropped);
			}
		}
	}
	return Report;
}
#endif
