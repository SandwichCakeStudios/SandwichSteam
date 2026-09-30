// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"

/** Native (non-Blueprint) events of the ISteamUserStats backend. Always broadcast on the game thread. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FSteamBackendStatsReceived, bool /*bSuccess*/, int32 /*NativeResult*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSteamBackendStatsStored, bool /*bSuccess*/, int32 /*NativeResult*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSteamBackendAchievementStored, FName /*ApiName*/, int32 /*CurrentProgress*/, int32 /*MaxProgress*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSteamBackendAchievementIcon, FName /*ApiName*/, int32 /*ImageHandle*/);

/**
 * Raw ISteamUserStats access shared by the Stats and Achievements modules. The header has no Steamworks
 * types, so dependents do not need the SDK. Query methods run on the game thread and only while Steam is Ready.
 * Raw callbacks copy their payload and are re-broadcast on the game thread through the dispatcher, and only while the
 * backend is alive (the subsystem that owns it resets it in ShutdownFeature).
 */
class SANDWICHSTEAMSTATS_API FSteamUserStatsBackend : public TSharedFromThis<FSteamUserStatsBackend, ESPMode::ThreadSafe>
{
public:
	using FPtr = TSharedPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe>;

	/** Creates the backend and registers the raw callbacks. Never construct it any other way. */
	static FPtr Create(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	explicit FSteamUserStatsBackend(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);
	~FSteamUserStatsBackend();

	FSteamUserStatsBackend(const FSteamUserStatsBackend&) = delete;
	FSteamUserStatsBackend& operator=(const FSteamUserStatsBackend&) = delete;

	/** True when the SDK in use still has (and needs) ISteamUserStats::RequestCurrentStats. Otherwise stats are ready at startup. */
	static constexpr bool NeedsStatsRequest() { return SANDWICHSTEAM_WITH_STATS_REQUEST != 0; }

	/** Asks Steam for the stats of the local user. The result arrives through OnStatsReceived. */
	bool RequestCurrentStats() const;

	// Stats
	bool GetInt(FName ApiName, int32& OutValue) const;
	bool GetFloat(FName ApiName, float& OutValue) const;
	bool SetInt(FName ApiName, int32 Value) const;
	bool SetFloat(FName ApiName, float Value) const;
	bool UpdateAvgRate(FName ApiName, float CountThisSession, double SessionSeconds) const;

	/** Uploads changed stats and achievements. The result arrives through OnStatsStored. */
	bool StoreStats() const;

	/** Resets every stat (and achievements when requested). Debug only. */
	bool ResetAllStats(bool bAchievementsToo) const;

	// Achievements
	bool GetAchievement(FName ApiName, bool& bOutUnlocked, int64& OutUnlockUnixTime) const;
	bool SetAchievement(FName ApiName) const;
	bool ClearAchievement(FName ApiName) const;
	bool IndicateProgress(FName ApiName, uint32 Current, uint32 Max) const;

	/** Image handle of the achievement icon, 0 when Steam does not have it yet (OnAchievementIcon follows when it is fetched). */
	int32 GetAchievementIconHandle(FName ApiName) const;

	/** Localized Steam attribute of an achievement: "name", "desc" or "hidden" ("1"/"0"). Empty when unknown. */
	FString GetAchievementAttribute(FName ApiName, const TCHAR* Key) const;

	int32 GetNumAchievements() const;
	FName GetAchievementApiName(int32 Index) const;

	/** Asks Steam for global achievement percentages. OnDone runs on the game thread (bSuccess), never after the backend is destroyed. */
	bool RequestGlobalPercentages(TFunction<void(bool /*bSuccess*/)> OnDone) const;
	bool GetAchievedPercent(FName ApiName, float& OutPercent) const;

	/** Steam App ID the SDK runs as. 0 when unavailable. */
	uint32 GetAppId() const;

	/** Steam ID of the local user. 0 when unavailable. */
	uint64 GetLocalSteamId() const;

	FSteamBackendStatsReceived OnStatsReceived;
	FSteamBackendStatsStored OnStatsStored;
	FSteamBackendAchievementStored OnAchievementStored;
	FSteamBackendAchievementIcon OnAchievementIcon;

private:
	struct FImpl;

	void BindCallbacks();

	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
	TUniquePtr<FImpl> Impl;
};
