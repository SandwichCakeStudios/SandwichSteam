// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamSDK.h"

/**
 * Raw Steamworks side of the Overlay feature (ISteamFriends activation calls, ISteamUtils overlay flags).
 * The overlay opened/closed event is not raised here: it comes from the OSS ExternalUI delegate.
 * Game thread only, only while Steam is Ready.
 */
class FSteamOverlayBackend
{
public:
	bool IsOverlayEnabled() const;
	uint32 GetAppId() const;

	/** Steam overlay dialog name: "friends", "community", "players", "settings", "officialgamegroup", "stats", "achievements". */
	void OpenDialog(const FString& DialogName) const;

	/** Steam overlay user dialog name: "steamid" (profile), "chat", "friendadd". */
	void OpenUserDialog(const FString& DialogName, int64 SteamId) const;

	/** AppId 0 = this game. Flag is an ESteamOverlayStoreFlag value. */
	void OpenStore(uint32 AppId, int32 Flag) const;

	void OpenWebPage(const FString& Url, bool bModal) const;
	void OpenInviteDialog(int64 LobbyId) const;
};
