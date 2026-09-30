// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamId.h"
#include "Core/SteamSDK.h"
#include "UObject/WeakObjectPtr.h"

class USteamScreenshotsSubsystem;

/**
 * Raw ISteamScreenshots calls. Methods run on the game thread and must only be called while Steam is Ready.
 * The two callbacks run on Steam's callback thread: they copy their payload and dispatch to the subsystem.
 * Destroying the backend gives the screenshot key back to Steam (unhooks).
 */
class FSteamScreenshotsBackend
{
public:
	FSteamScreenshotsBackend(USteamScreenshotsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);
	~FSteamScreenshotsBackend();

	FSteamScreenshotsBackend(const FSteamScreenshotsBackend&) = delete;
	FSteamScreenshotsBackend& operator=(const FSteamScreenshotsBackend&) = delete;

	/** Asks Steam to take a screenshot (as if the screenshot key was pressed). */
	void TriggerScreenshot() const;

	/** Hook mode: while hooked Steam does not capture, it sends ScreenshotRequested_t and the game must answer with WriteScreenshot. */
	void SetHooked(bool bHooked);
	bool IsHooked() const { return bHooked; }

	/** Hands an image to Steam (tightly packed R G B). Returns the screenshot handle, 0 when Steam refused. */
	uint32 WriteScreenshot(TConstArrayView<uint8> Rgb, int32 Width, int32 Height) const;

	void SetLocation(uint32 Handle, const FString& Location) const;
	void TagUser(uint32 Handle, FSteamId User) const;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamScreenshotsBackend, OnScreenshotRequested, ScreenshotRequested_t, RequestedCallback);
	STEAM_CALLBACK_MANUAL(FSteamScreenshotsBackend, OnScreenshotReady, ScreenshotReady_t, ReadyCallback);
#endif

	TWeakObjectPtr<USteamScreenshotsSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
	bool bHooked = false;
};
