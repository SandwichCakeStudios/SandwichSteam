// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceBackend.h"
#include "Core/SteamBackend.h"
#include "Core/SteamSDK.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "OnlineSubsystem.h"

FSteamVoiceBackend::FSteamVoiceBackend(UGameInstance* InGameInstance)
	: GameInstance(InGameInstance)
{
	if (IOnlineSubsystem* OSS = SandwichSteam::GetSteamOSS(InGameInstance))
	{
		Voice = OSS->GetVoiceInterface();
	}
}

int32 FSteamVoiceBackend::GetLocalUserNum() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const ULocalPlayer* LocalPlayer = Instance ? Instance->GetFirstGamePlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetControllerId() : 0;
}

FSteamId FSteamVoiceBackend::GetLocalSteamId() const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (SandwichSteam::IsSteamClientReady(GameInstance.Get()))
	{
		if (ISteamUser* User = SteamUser())
		{
			return FSteamId(static_cast<int64>(User->GetSteamID().ConvertToUint64()));
		}
	}
#endif
	return FSteamId();
}

bool FSteamVoiceBackend::StartTalking()
{
	if (!Voice.IsValid())
	{
		return false;
	}
	Voice->StartNetworkedVoice(static_cast<uint8>(GetLocalUserNum()));
	return true;
}

bool FSteamVoiceBackend::StopTalking()
{
	if (!Voice.IsValid())
	{
		return false;
	}
	Voice->StopNetworkedVoice(static_cast<uint8>(GetLocalUserNum()));
	return true;
}

bool FSteamVoiceBackend::IsHeadsetPresent()
{
	return Voice.IsValid() && Voice->IsHeadsetPresent(GetLocalUserNum());
}

bool FSteamVoiceBackend::IsLocalTalking()
{
	return Voice.IsValid() && Voice->IsLocalPlayerTalking(GetLocalUserNum());
}

bool FSteamVoiceBackend::IsRemoteTalking(const FUniqueNetId& Player)
{
	return Voice.IsValid() && Voice->IsRemotePlayerTalking(Player);
}

bool FSteamVoiceBackend::SetMuted(const FUniqueNetId& Player, bool bMuted, bool bSystemWide)
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

bool FSteamVoiceBackend::IsMuted(const FUniqueNetId& Player) const
{
	return Voice.IsValid() && Voice->IsMuted(GetLocalUserNum(), Player);
}

bool FSteamVoiceBackend::IsBlockedOnSteam(FSteamId Player) const
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (Player.IsValid() && SandwichSteam::IsSteamClientReady(GameInstance.Get()))
	{
		if (ISteamFriends* Friends = SteamFriends())
		{
			return Friends->GetFriendRelationship(CSteamID(static_cast<uint64>(Player.Value))) == k_EFriendRelationshipBlocked;
		}
	}
#endif
	return false;
}
