// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"

/** C++ event: a StoreStats round trip finished (game thread). */
DECLARE_MULTICAST_DELEGATE_OneParam(FSteamNativeStatsStored, const FSteamResult& /*Result*/);

/** C++ event: a stat was changed through the plugin (game thread). Value is the new value. Used by the Achievements module for progress toasts. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FSteamNativeStatChanged, FName /*ApiName*/, double /*NewValue*/);

/** C++ event: the stats became ready (game thread). */
DECLARE_MULTICAST_DELEGATE(FSteamNativeStatsReady);
