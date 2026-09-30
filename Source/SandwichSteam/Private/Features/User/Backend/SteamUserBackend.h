// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "Core/SteamSDK.h"
#include "Features/User/SteamAvatarCache.h"
#include "UObject/WeakObjectPtr.h"

class USteamUserSubsystem;

/**
 * Raw Steamworks side of the User feature (ISteamUser, ISteamFriends, ISteamUtils, ISteamApps).
 * Query methods run on the game thread and must only be called while Steam is Ready.
 * Raw callbacks copy their payload and dispatch to USteamUserSubsystem on the game thread.
 */
class FSteamUserBackend
{
public:
	FSteamUserBackend(USteamUserSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	FSteamUserBackend(const FSteamUserBackend&) = delete;
	FSteamUserBackend& operator=(const FSteamUserBackend&) = delete;

	/** True when the SDK in use has ISteamUser::GetAuthTicketForWebApi. */
	static constexpr bool SupportsWebApiTicket() { return SANDWICHSTEAM_WITH_WEBAPI_TICKET != 0; }

	bool IsLoggedOn() const;
	FSteamId GetLocalSteamId() const;
	FString GetPersonaName() const;
	int32 GetSteamLevel() const;

	bool IsSubscribed() const;
	bool IsSubscribedApp(uint32 AppId) const;
	FSteamId GetAppOwner() const;
	int64 GetEarliestPurchaseUnixTime(uint32 AppId) const;

	/**
	 * Asks Steam for the avatar. Ready fills OutPixels. Loading also requests the user's information from Steam
	 * (needed for non-friends); completion arrives through the persona / avatar callbacks.
	 */
	ESteamAvatarFetch FetchAvatar(FSteamId UserId, ESteamAvatarSize Size, FSteamAvatarPixels& OutPixels) const;

	/** Starts a Web API ticket request. Returns the ticket handle, 0 when it could not be started. */
	uint32 RequestWebApiTicket(const FString& Identity) const;

	/** Cancels a ticket previously returned by RequestWebApiTicket(). */
	void CancelTicket(uint32 TicketHandle) const;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamUserBackend, OnPersonaStateChange, PersonaStateChange_t, PersonaStateChangeCallback);
	STEAM_CALLBACK_MANUAL(FSteamUserBackend, OnAvatarImageLoaded, AvatarImageLoaded_t, AvatarImageLoadedCallback);
#if SANDWICHSTEAM_WITH_WEBAPI_TICKET
	STEAM_CALLBACK_MANUAL(FSteamUserBackend, OnWebApiTicketResponse, GetTicketForWebApiResponse_t, WebApiTicketCallback);
#endif
#endif

	TWeakObjectPtr<USteamUserSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};
