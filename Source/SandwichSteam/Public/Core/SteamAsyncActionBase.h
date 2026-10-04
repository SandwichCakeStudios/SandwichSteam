// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Templates/SubclassOf.h"
#include "Core/SteamResult.h"
#include "SteamAsyncActionBase.generated.h"

class UGameInstance;
class USteamFeatureSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamAsyncSuccessDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncFailureDelegate, const FSteamResult&, Result);

/**
 * Base for every Blueprint async node of the plugin (Steam|<Feature>).
 *
 * Handles: resolving the GameInstance and feature subsystem, failing fast when the feature is inactive,
 * a timeout (Steam.Error.Timeout), cancellation when the feature deactivates or shuts down
 * (Steam.Error.Cancelled), and SetReadyToDestroy() exactly once on every path.
 *
 * A derived node:
 *  1. Has a static factory that creates the action, stores its arguments and calls SetWorldContext().
 *  2. Overrides GetFeatureClass() and StartRequest(). StartRequest() runs only when the feature is active.
 *  3. Calls FinishSuccess() or FinishFailure() (game thread) when the request completes.
 * A node that returns data adds its own success delegate and overrides BroadcastSuccess().
 * Bind Steam completions with weak references only; a cancelled action may already be finished.
 */
UCLASS(Abstract)
class SANDWICHSTEAM_API USteamAsyncActionBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fired when the request succeeded. */
	UPROPERTY(BlueprintAssignable, Category = "Steam", meta = (ToolTip = "Called when the Steam request succeeded."))
	FSteamAsyncSuccessDelegate OnSuccess;

	/** Fired when the request failed, timed out or was cancelled. */
	UPROPERTY(BlueprintAssignable, Category = "Steam", meta = (ToolTip = "Called when the Steam request failed, timed out or was cancelled. Result.ErrorTag says why."))
	FSteamAsyncFailureDelegate OnFailure;

	//~ Begin UBlueprintAsyncActionBase
	virtual void Activate() override;
	//~ End UBlueprintAsyncActionBase

	/** True once FinishSuccess() or FinishFailure() ran. */
	bool IsFinished() const { return bFinished; }

protected:
	/** Factories call this with their WorldContextObject parameter. */
	void SetWorldContext(const UObject* InWorldContext) { WorldContext = InWorldContext; }

	/** Feature subsystem that must be active for this request. */
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const PURE_VIRTUAL(USteamAsyncActionBase::GetFeatureClass, return nullptr;);

	/** Start the Steam request. Runs on the game thread, only while the feature is active. */
	virtual void StartRequest() PURE_VIRTUAL(USteamAsyncActionBase::StartRequest,);

	/** Broadcast the success delegate(s). Override to pass result data. */
	virtual void BroadcastSuccess() { OnSuccess.Broadcast(); }

	/** Broadcast the failure delegate(s). */
	virtual void BroadcastFailure(const FSteamResult& Result) { OnFailure.Broadcast(Result); }

	/** Complete successfully. No-op if already finished. */
	void FinishSuccess();

	/** Complete with a failure. No-op if already finished. */
	void FinishFailure(const FSteamResult& Result);

	UGameInstance* GetOwningGameInstance() const { return GameInstance.Get(); }
	USteamFeatureSubsystem* GetFeature() const { return Feature.Get(); }

	/** Seconds until the request fails with Steam.Error.Timeout. 0 disables the timeout. Set in the factory. */
	float TimeoutSeconds = 30.0f;

private:
	UFUNCTION()
	void HandleFeatureActiveChanged(bool bActive);

	void Cleanup();

	TWeakObjectPtr<const UObject> WorldContext;
	TWeakObjectPtr<UGameInstance> GameInstance;
	TWeakObjectPtr<USteamFeatureSubsystem> Feature;
	FTSTicker::FDelegateHandle TimeoutHandle;
	bool bFinished = false;
};
