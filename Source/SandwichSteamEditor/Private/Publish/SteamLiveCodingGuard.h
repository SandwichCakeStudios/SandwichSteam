// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Switches Live Coding off while UAT packages and puts it back afterwards (UAT/UBT cannot build while Live Coding is
 * enabled in the running editor). Remembers both the saved setting (LiveCodingSettings.bEnabled) and the session state,
 * and only restores what it changed. Windows only: on other platforms Acquire does nothing.
 * Release runs from the destructor too, so an abandoned publish job never leaves Live Coding switched off.
 */
class FSteamLiveCodingGuard
{
public:
	FSteamLiveCodingGuard() = default;
	FSteamLiveCodingGuard(const FSteamLiveCodingGuard&) = delete;
	FSteamLiveCodingGuard& operator=(const FSteamLiveCodingGuard&) = delete;
	~FSteamLiveCodingGuard();

	/** Disables Live Coding if it is on. Returns true when something was changed; OutMessage is a log line for the publish log (empty when there was nothing to do). */
	bool Acquire(FString& OutMessage);

	/** Restores what Acquire changed. Returns true when something was restored; OutMessage is a log line. Safe to call repeatedly. */
	bool Release(FString& OutMessage);

	bool IsHeld() const { return bHeld; }

private:
	bool bHeld = false;
	bool bWasEnabledByDefault = false;
	bool bWasEnabledForSession = false;
};
