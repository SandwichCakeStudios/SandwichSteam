// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceOssFallback.h"
#include "Core/SteamBackend.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "OnlineSubsystem.h"

FSteamVoiceOssFallback::FSteamVoiceOssFallback(UGameInstance* InGameInstance)
	: GameInstance(InGameInstance)
{
	if (IOnlineSubsystem* OSS = SandwichSteam::GetSteamOSS(InGameInstance))
	{
		Voice = OSS->GetVoiceInterface();
	}
}

int32 FSteamVoiceOssFallback::GetLocalUserNum() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const ULocalPlayer* LocalPlayer = Instance ? Instance->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetControllerId() : 0;
}

bool FSteamVoiceOssFallback::StartTalking()
{
	if (!Voice.IsValid())
	{
		return false;
	}
	Voice->StartNetworkedVoice(static_cast<uint8>(GetLocalUserNum()));
	return true;
}

bool FSteamVoiceOssFallback::StopTalking()
{
	if (!Voice.IsValid())
	{
		return false;
	}
	Voice->StopNetworkedVoice(static_cast<uint8>(GetLocalUserNum()));
	return true;
}

bool FSteamVoiceOssFallback::IsHeadsetPresent()
{
	return Voice.IsValid() && Voice->IsHeadsetPresent(GetLocalUserNum());
}

bool FSteamVoiceOssFallback::IsLocalTalking()
{
	return Voice.IsValid() && Voice->IsLocalPlayerTalking(GetLocalUserNum());
}

bool FSteamVoiceOssFallback::IsRemoteTalking(const FUniqueNetId& Player)
{
	return Voice.IsValid() && Voice->IsRemotePlayerTalking(Player);
}

bool FSteamVoiceOssFallback::SetMuted(const FUniqueNetId& Player, bool bMuted, bool bSystemWide)
{
	if (!Voice.IsValid())
	{
		return false;
	}

	const uint8 LocalUserNum = static_cast<uint8>(GetLocalUserNum());
	return bMuted
		? Voice->MuteRemoteTalker(LocalUserNum, Player, bSystemWide)
		: Voice->UnmuteRemoteTalker(LocalUserNum, Player, bSystemWide);
}

bool FSteamVoiceOssFallback::IsMuted(const FUniqueNetId& Player) const
{
	return Voice.IsValid() && Voice->IsMuted(GetLocalUserNum(), Player);
}
