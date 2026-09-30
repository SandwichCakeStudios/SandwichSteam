// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "SteamAchievementTypes.h"
#include "SteamAchievementsSubsystem.generated.h"

class FSteamProgressThrottle;
class FSteamUserStatsBackend;
class USteamAppDefinition;
class USteamStatsSubsystem;
struct FSteamAchievementDef;

/**
 * Steam achievements of the local user. Client only. Needs the Stats feature (same module family): it owns the
 * ISteamUserStats backend and the moment the stats are ready.
 *
 * Unlocks are stored at once (Steam shows its toast). Unlocks requested before the stats arrived are queued.
 * A stat with an achievement attached (Progress Stat in the App Definition) shows the Steam progress toast on its own.
 * Do not mix with the engine's OSS achievement nodes: use one API for the same achievements.
 */
UCLASS()
class SANDWICHSTEAMACHIEVEMENTS_API USteamAchievementsSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Achievements subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamAchievementsSubsystem* Get(const UObject* WorldContext);

	USteamAchievementsSubsystem();
	virtual ~USteamAchievementsSubsystem() override;

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/**
	 * Unlocks an achievement and uploads it at once. Already unlocked achievements succeed without a Steam call.
	 * Before the stats arrived from Steam the unlock is queued and happens automatically.
	 */
	FSteamResult Unlock(const FGameplayTag& AchievementTag);
	FSteamResult Unlock(FName ApiName);

	/** Locks an achievement again. Available in non-Shipping builds only (Steam.Error.NotSupported otherwise). */
	FSteamResult Clear(const FGameplayTag& AchievementTag);
	FSteamResult Clear(FName ApiName);

	/** Unlock state and time. Fails until the stats are ready. */
	FSteamResult GetState(const FGameplayTag& AchievementTag, bool& bOutUnlocked, FDateTime& OutUnlockTime) const;
	FSteamResult GetState(FName ApiName, bool& bOutUnlocked, FDateTime& OutUnlockTime) const;

	/** Name, description, state and progress of one achievement. Progress needs a row with a Progress Stat in the App Definition. */
	FSteamResult GetInfo(const FGameplayTag& AchievementTag, FSteamAchievementInfo& OutInfo) const;
	FSteamResult GetInfo(FName ApiName, FSteamAchievementInfo& OutInfo) const;

	/** Every achievement of the App Definition (or every achievement Steam reports when there is no definition). Empty until the stats are ready. */
	void GetAllInfo(TArray<FSteamAchievementInfo>& OutInfos) const;

	/**
	 * Shows the Steam progress toast for an achievement with a progress stat. Throttled: shown once per step of
	 * the "Progress Notify Step" setting, never at or above the maximum.
	 * By name, MaxValue 0 uses the Progress Max of the App Definition row; pass it for achievements without a row.
	 */
	FSteamResult IndicateProgress(const FGameplayTag& AchievementTag, int32 CurrentValue);
	FSteamResult IndicateProgress(FName ApiName, int32 CurrentValue, int32 MaxValue = 0);

	/** Loads the icon of the achievement (locked or unlocked version, whichever is current). The delegate runs once, possibly before this returns. */
	FSteamResult RequestIcon(const FGameplayTag& AchievementTag, FSteamAchievementIconDelegate OnComplete);
	FSteamResult RequestIcon(FName ApiName, FSteamAchievementIconDelegate OnComplete);

	/** Asks Steam how many players unlocked each achievement. Read the values with GetGlobalPercent afterwards. */
	FSteamResult RequestGlobalPercentages(FSteamAchievementPercentagesDelegate OnComplete);

	/** Percentage of players that unlocked the achievement (0-100). False until RequestGlobalPercentages succeeded. */
	bool GetGlobalPercent(const FGameplayTag& AchievementTag, float& OutPercent) const;
	bool GetGlobalPercent(FName ApiName, float& OutPercent) const;

	/** Called when an achievement was unlocked and stored. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Achievements", meta = (ToolTip = "Called when an achievement was unlocked and stored by Steam."))
	FOnSteamAchievementUnlocked OnAchievementUnlocked;

	/** Called when Steam showed progress towards an achievement. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Achievements", meta = (ToolTip = "Called when Steam showed progress towards an achievement."))
	FOnSteamAchievementProgress OnAchievementProgress;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Achievements.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	struct FPendingIcon
	{
		FName ApiName;
		FSteamAchievementIconDelegate Callback;
	};

	FSteamResult ResolveTag(const FGameplayTag& AchievementTag, FName& OutApiName) const;
	FSteamResult RequireStatsReady() const;
	FSteamResult IndicateProgressInternal(FName ApiName, int32 CurrentValue, int32 MaxValue);
	void BuildInfo(FName ApiName, const FSteamAchievementDef* Def, FSteamAchievementInfo& OutInfo) const;
	void DeliverIcon(FName ApiName, int32 ImageHandle, const FSteamAchievementIconDelegate& Callback);

	void HandleStatsReady();
	void HandleStatChanged(FName StatApiName, double NewValue);
	void HandleAchievementStored(FName ApiName, int32 Current, int32 Max);
	void HandleAchievementIcon(FName ApiName, int32 ImageHandle);
	void HandlePercentagesReady(bool bSuccess);

	UPROPERTY(Transient)
	TObjectPtr<USteamStatsSubsystem> Stats;

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	/** Icons handed out so far. Bounded by the number of achievements. Dropped when the unlock state changes. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> IconCache;

	TWeakPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe> WeakBackend;
	TSharedPtr<FSteamProgressThrottle> Throttle;

	/** Progress stat API name -> indices into Definition->Achievements. */
	TMap<FName, TArray<int32>> AchievementsByProgressStat;

	TArray<FName> QueuedUnlocks;
	TArray<FPendingIcon> PendingIcons;
	TArray<FSteamAchievementPercentagesDelegate> PendingPercentages;
	TMap<FName, float> GlobalPercents;
	bool bPercentagesRequested = false;
};
