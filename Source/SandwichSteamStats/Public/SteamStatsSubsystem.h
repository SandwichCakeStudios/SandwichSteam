// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/SteamFeatureSubsystem.h"
#include "SteamStatsTypes.h"
#include "SteamUserStatsBackend.h"
#include "SteamStatsSubsystem.generated.h"

class USteamAppDefinition;
struct FSteamStatDef;

/** Blueprint event: the stats of the local user were received from Steam and can be read and written. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamStatsReady);

/**
 * Steam stats of the local user (ISteamUserStats). Client only.
 *
 * Stats are read and written by gameplay tag (rows of the Steam App Definition) or by API name. Writes made before Steam
 * delivered the stats are queued and applied when they arrive. Changes are uploaded in coalesced StoreStats calls.
 * Owns the ISteamUserStats backend that the Achievements module shares.
 */
UCLASS()
class SANDWICHSTEAMSTATS_API USteamStatsSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Stats subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamStatsSubsystem* Get(const UObject* WorldContext);

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** True once Steam delivered the stats of the local user. Writes before that are queued. */
	UFUNCTION(BlueprintPure, Category = "Steam|Stats", meta = (ToolTip = "True once Steam delivered the stats of the local user. Stat writes made earlier are queued and applied automatically."))
	bool AreStatsReady() const { return bStatsReady; }

	/** Called once when the stats become ready. Not called again after a reload. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Stats", meta = (ToolTip = "Called once when the stats of the local user were received from Steam."))
	FOnSteamStatsReady OnStatsReady;

	/** The App Definition in use, or nullptr. Stats by tag need it, stats by API name do not. */
	USteamAppDefinition* GetDefinition() const { return Definition; }

	// Reads. Fail with NotInitialized until the stats are ready.
	FSteamResult GetInt(const FGameplayTag& StatTag, int32& OutValue) const;
	FSteamResult GetInt(FName ApiName, int32& OutValue) const;
	FSteamResult GetFloat(const FGameplayTag& StatTag, float& OutValue) const;
	FSteamResult GetFloat(FName ApiName, float& OutValue) const;

	// Writes. Queued until the stats are ready. Values are clamped to the range of the definition row.
	FSteamResult SetInt(const FGameplayTag& StatTag, int32 Value);
	FSteamResult SetInt(FName ApiName, int32 Value);
	FSteamResult SetFloat(const FGameplayTag& StatTag, float Value);
	FSteamResult SetFloat(FName ApiName, float Value);
	FSteamResult AddInt(const FGameplayTag& StatTag, int32 Delta);
	FSteamResult AddInt(FName ApiName, int32 Delta);
	FSteamResult AddFloat(const FGameplayTag& StatTag, float Delta);
	FSteamResult AddFloat(FName ApiName, float Delta);

	/** Average-rate stat: CountThisSession events during SessionSeconds. */
	FSteamResult UpdateAvgRate(const FGameplayTag& StatTag, float CountThisSession, float SessionSeconds);
	FSteamResult UpdateAvgRate(FName ApiName, float CountThisSession, float SessionSeconds);

	/** Uploads changed stats now instead of waiting for the flush interval. Follow OnStatsStoredNative for the result. */
	FSteamResult StoreStatsNow();

	/** True while changes wait for an upload or an upload is in flight. */
	bool IsStorePending() const;

	/**
	 * Tells the subsystem that stats or achievements were changed outside of it (the Achievements module).
	 * Urgent changes are uploaded at once.
	 */
	void MarkStatsDirty(bool bUrgent);

	/** The shared ISteamUserStats backend. Null while the feature is inactive. */
	FSteamUserStatsBackend::FPtr GetBackend() const { return Backend; }

	/** C++ events for sibling modules and async nodes. */
	FSteamNativeStatsReady OnStatsReadyNative;
	FSteamNativeStatsStored OnStatsStoredNative;
	FSteamNativeStatChanged OnStatChangedNative;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Stats.Dump. */
	FString BuildDebugString() const;

	/** Resets every stat (and achievements when requested). Debug only. */
	FSteamResult DebugResetAll(bool bAchievementsToo);

	/** Writes Saved/SandwichSteam/Schema_<AppId>.json with the achievements Steam reports and the stats of the definition. */
	FSteamResult DebugExportSchema(FString& OutFilePath) const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	enum class EOp : uint8
	{
		SetInt,
		AddInt,
		SetFloat,
		AddFloat,
		AvgRate
	};

	struct FQueuedOp
	{
		FName ApiName;
		EOp Op = EOp::SetInt;
		double Value = 0.0;
		double Seconds = 0.0;
	};

	/** Type a stat must have. Rows of the definition decide when they exist, otherwise the operation does. */
	enum class EKind : uint8
	{
		Int,
		Float
	};

	FSteamResult ResolveTag(const FGameplayTag& StatTag, EKind Kind, FName& OutApiName) const;
	FSteamResult ReadStat(FName ApiName, EKind Kind, double& OutValue) const;
	FSteamResult Write(FName ApiName, EOp Op, double Value, double Seconds);
	bool ApplyOp(const FQueuedOp& Op);
	double Clamp(FName ApiName, double Value) const;
	void ApplyQueuedOps();

	void HandleStatsReceived(bool bSuccess, int32 NativeResult);
	void HandleStatsStored(bool bSuccess, int32 NativeResult);
	void MarkStatsReady();

	void TryFlush();
	void StartStore();
	void EnsureFlushTicker();
	bool TickFlush(float DeltaTime);
	void FlushBlocking();

	FSteamUserStatsBackend::FPtr Backend;

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	TArray<FQueuedOp> QueuedOps;

	struct FFlushState;
	TSharedPtr<FFlushState> Flush;

	FTSTicker::FDelegateHandle FlushTickerHandle;
	FTSTicker::FDelegateHandle ReadyTickerHandle;
	FDelegateHandle PreExitHandle;

	bool bStatsReady = false;
	bool bReadyBroadcast = false;
	bool bQueueOverflowLogged = false;
};
