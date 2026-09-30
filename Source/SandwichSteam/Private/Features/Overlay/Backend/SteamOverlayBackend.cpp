// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Overlay/Backend/SteamOverlayBackend.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

bool FSteamOverlayBackend::IsOverlayEnabled() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils && Utils->IsOverlayEnabled();
}

uint32 FSteamOverlayBackend::GetAppId() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? Utils->GetAppID() : 0;
}

void FSteamOverlayBackend::OpenDialog(const FString& DialogName) const
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		Friends->ActivateGameOverlay(TCHAR_TO_UTF8(*DialogName));
	}
}

void FSteamOverlayBackend::OpenUserDialog(const FString& DialogName, int64 SteamId) const
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		Friends->ActivateGameOverlayToUser(TCHAR_TO_UTF8(*DialogName), CSteamID(static_cast<uint64>(SteamId)));
	}
}

void FSteamOverlayBackend::OpenStore(uint32 AppId, int32 Flag) const
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		Friends->ActivateGameOverlayToStore(AppId != 0 ? AppId : GetAppId(), static_cast<EOverlayToStoreFlag>(Flag));
	}
}

void FSteamOverlayBackend::OpenWebPage(const FString& Url, bool bModal) const
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		Friends->ActivateGameOverlayToWebPage(TCHAR_TO_UTF8(*Url),
			bModal ? k_EActivateGameOverlayToWebPageMode_Modal : k_EActivateGameOverlayToWebPageMode_Default);
	}
}

void FSteamOverlayBackend::OpenInviteDialog(int64 LobbyId) const
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		Friends->ActivateGameOverlayInviteDialog(CSteamID(static_cast<uint64>(LobbyId)));
	}
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

bool FSteamOverlayBackend::IsOverlayEnabled() const { return false; }
uint32 FSteamOverlayBackend::GetAppId() const { return 0; }
void FSteamOverlayBackend::OpenDialog(const FString&) const {}
void FSteamOverlayBackend::OpenUserDialog(const FString&, int64) const {}
void FSteamOverlayBackend::OpenStore(uint32, int32) const {}
void FSteamOverlayBackend::OpenWebPage(const FString&, bool) const {}
void FSteamOverlayBackend::OpenInviteDialog(int64) const {}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
