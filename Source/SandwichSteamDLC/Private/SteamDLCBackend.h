// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamSDK.h"
#include "SteamDLCTypes.h"
#include "UObject/WeakObjectPtr.h"

class USteamDLCSubsystem;

/**
 * Raw ISteamApps DLC calls. Methods run on the game thread and must only be called while Steam is Ready.
 * DlcInstalled_t runs on Steam's callback thread: it copies the payload and dispatches to the subsystem.
 */
class FSteamDLCBackend
{
public:
	FSteamDLCBackend(USteamDLCSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	FSteamDLCBackend(const FSteamDLCBackend&) = delete;
	FSteamDLCBackend& operator=(const FSteamDLCBackend&) = delete;

	/** Every DLC Steam knows for this game (Tag is not filled in). */
	void ListDLC(TArray<FSteamDLCInfo>& OutDLC) const;

	bool IsOwned(uint32 AppId) const;
	bool IsInstalled(uint32 AppId) const;
	void Install(uint32 AppId) const;
	void Uninstall(uint32 AppId) const;
	FSteamDLCProgress GetProgress(uint32 AppId) const;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamDLCBackend, OnDlcInstalled, DlcInstalled_t, DlcInstalledCallback);
#endif

	TWeakObjectPtr<USteamDLCSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};
