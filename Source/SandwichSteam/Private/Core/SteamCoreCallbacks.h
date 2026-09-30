// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamCallResult.h"
#include "UObject/WeakObjectPtr.h"

class USteamCoreSubsystem;

#if SANDWICHSTEAM_WITH_STEAMWORKS

/**
 * Raw Steam callbacks that drive USteamCoreSubsystem's state machine (client only).
 * The handlers run on Steam's callback thread: they copy the payload and dispatch to the game thread.
 * Registered only when the Steam Online Subsystem is up, so Steam is already initialized.
 */
class FSteamCoreCallbacks
{
public:
	FSteamCoreCallbacks(USteamCoreSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	FSteamCoreCallbacks(const FSteamCoreCallbacks&) = delete;
	FSteamCoreCallbacks& operator=(const FSteamCoreCallbacks&) = delete;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Issues a harmless Steam call whose result callback reveals the thread that pumps Steam callbacks. */
	void RunThreadCheck();
#endif

private:
	STEAM_CALLBACK_MANUAL(FSteamCoreCallbacks, OnSteamServersConnected, SteamServersConnected_t, ServersConnectedCallback);
	STEAM_CALLBACK_MANUAL(FSteamCoreCallbacks, OnSteamServersDisconnected, SteamServersDisconnected_t, ServersDisconnectedCallback);
	STEAM_CALLBACK_MANUAL(FSteamCoreCallbacks, OnSteamShutdown, SteamShutdown_t, ShutdownCallback);

	TWeakObjectPtr<USteamCoreSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

#if SANDWICHSTEAM_WITH_DEBUG
	TSteamCallResult<LeaderboardFindResult_t>::FPendingList ThreadCheckPending;
#endif
};

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
