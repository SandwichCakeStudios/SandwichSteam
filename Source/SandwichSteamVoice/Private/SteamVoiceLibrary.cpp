// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "SteamVoiceSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "VoiceLibNotAvailable", "The Voice feature is not available in this game instance."));
	}
}

FSteamResult USteamVoiceLibrary::SetSteamVoiceEnabled(const UObject* WorldContextObject, bool bEnabled)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->SetVoiceEnabled(bEnabled) : NotAvailable();
}

bool USteamVoiceLibrary::IsSteamVoiceEnabled(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice && Voice->IsVoiceEnabled();
}

FSteamResult USteamVoiceLibrary::SetSteamVoiceMode(const UObject* WorldContextObject, ESteamVoiceMode Mode)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->SetVoiceMode(Mode) : NotAvailable();
}

ESteamVoiceMode USteamVoiceLibrary::GetSteamVoiceMode(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->GetVoiceMode() : ESteamVoiceMode::PushToTalk;
}

FSteamResult USteamVoiceLibrary::StartSteamTalking(const UObject* WorldContextObject)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->StartTalking() : NotAvailable();
}

FSteamResult USteamVoiceLibrary::StopSteamTalking(const UObject* WorldContextObject)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->StopTalking() : NotAvailable();
}

FSteamResult USteamVoiceLibrary::BindSteamPushToTalk(const UObject* WorldContextObject, APlayerController* Controller)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->BindPushToTalk(Controller) : NotAvailable();
}

bool USteamVoiceLibrary::IsSteamPushToTalkBound(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice && Voice->IsPushToTalkBound();
}

bool USteamVoiceLibrary::IsSteamLocalPlayerTalking(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice && Voice->IsLocalTalking();
}

bool USteamVoiceLibrary::IsSteamHeadsetPresent(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice && Voice->IsHeadsetPresent();
}

bool USteamVoiceLibrary::IsSteamPlayerTalking(const UObject* WorldContextObject, FSteamId Player)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice && Voice->IsPlayerTalking(Player);
}

TArray<FSteamId> USteamVoiceLibrary::GetSteamTalkingPlayers(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->GetTalkingPlayers() : TArray<FSteamId>();
}

FSteamResult USteamVoiceLibrary::MuteSteamPlayer(const UObject* WorldContextObject, FSteamId Player, bool bSystemWide)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->MutePlayer(Player, bSystemWide) : NotAvailable();
}

FSteamResult USteamVoiceLibrary::UnmuteSteamPlayer(const UObject* WorldContextObject, FSteamId Player)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->UnmutePlayer(Player) : NotAvailable();
}

bool USteamVoiceLibrary::IsSteamPlayerMuted(const UObject* WorldContextObject, FSteamId Player)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice && Voice->IsPlayerMuted(Player);
}

TArray<FSteamId> USteamVoiceLibrary::GetSteamMutedPlayers(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->GetMutedPlayers() : TArray<FSteamId>();
}

FSteamResult USteamVoiceLibrary::SetSteamVoiceMasterVolume(const UObject* WorldContextObject, float Volume)
{
	USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->SetMasterVolume(Volume) : NotAvailable();
}

float USteamVoiceLibrary::GetSteamVoiceMasterVolume(const UObject* WorldContextObject)
{
	const USteamVoiceSubsystem* Voice = USteamVoiceSubsystem::Get(WorldContextObject);
	return Voice ? Voice->GetMasterVolume() : 1.0f;
}
