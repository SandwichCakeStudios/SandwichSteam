// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/SteamCoreSubsystem.h"
#include "Core/SteamResult.h"
#include "SteamFeatureSubsystem.generated.h"

/** Where a feature makes sense. A dedicated server never creates ClientOnly features; a client never creates ServerOnly features. */
UENUM(BlueprintType)
enum class ESteamFeatureScope : uint8
{
	ClientOnly,
	ServerOnly,
	Both
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamFeatureActiveChanged, bool, bActive);

/**
 * Base for every Steam feature (User, Stats, Sessions, ...).
 *
 * The base depends on USteamCoreSubsystem and follows its state: InitializeFeature() runs when Steam becomes Ready
 * and ShutdownFeature() when Steam leaves Ready (offline, shutdown, GameInstance teardown).
 * While inactive, public API methods must return failure through RequireActive().
 */
UCLASS(Abstract)
class SANDWICHSTEAM_API USteamFeatureSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** True when the feature is enabled and has a working Steam backend. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (ToolTip = "Returns true if this Steam feature is enabled and has a working backend."))
	bool IsFeatureActive() const { return bFeatureActive; }

	/** Called on the game thread whenever the feature becomes active or inactive. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Core", meta = (ToolTip = "Called when this Steam feature becomes active or inactive."))
	FOnSteamFeatureActiveChanged OnFeatureActiveChanged;

	/** Gameplay tag (Steam.Feature.*) identifying this feature. */
	virtual FGameplayTag GetFeatureTag() const PURE_VIRTUAL(USteamFeatureSubsystem::GetFeatureTag, return FGameplayTag(););

	/** Where this feature is created. Default: Both. */
	virtual ESteamFeatureScope GetFeatureScope() const { return ESteamFeatureScope::Both; }

	/**
	 * Call at the top of every public API method (game thread only).
	 * Returns true and a success result when the feature is active, otherwise false and a failure result whose tag
	 * reflects the Steam state (Unavailable, NotInitialized, Offline).
	 */
	bool RequireActive(FSteamResult& OutResult) const;

protected:
	/** Create the backend and bind delegates. Return true if the feature is usable. Runs when Steam becomes Ready. */
	virtual bool InitializeFeature() { return false; }

	/** Unbind delegates, cancel pending requests and release the backend. Runs when Steam leaves Ready. */
	virtual void ShutdownFeature() {}

	/** Core subsystem of this GameInstance. Valid between Initialize() and Deinitialize(). */
	USteamCoreSubsystem* GetCore() const { return Core; }

private:
	UFUNCTION()
	void HandleSteamStateChanged(ESteamState NewState, ESteamState OldState);

	void ApplySteamState(ESteamState State);
	void SetFeatureActive(bool bActive);

	UPROPERTY(Transient)
	TObjectPtr<USteamCoreSubsystem> Core;

	bool bFeatureActive = false;
};
