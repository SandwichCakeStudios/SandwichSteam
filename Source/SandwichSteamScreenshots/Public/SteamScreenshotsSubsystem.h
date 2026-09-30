// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamId.h"
#include "SteamScreenshotsSubsystem.generated.h"

class FSteamScreenshotsBackend;
class FSteamCallbackDispatcher;

/** Called when Steam finished adding a screenshot to the library. Handle is Steam's screenshot handle. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamScreenshotReady, int32, Handle, bool, bSuccess);

/**
 * Steam screenshots (ISteamScreenshots). Client only.
 *
 * Two ways to get a screenshot into the Steam library:
 *  - Trigger Screenshot: Steam captures the screen itself (also what the screenshot key does when hook mode is off).
 *  - Capture Viewport To Steam: the game captures its own viewport (no overlay, no OS windows), converts it on a worker
 *    task and hands it to Steam.
 * Hook mode (settings, off by default): the game owns the screenshot key. Steam then asks the game for every
 * screenshot (ScreenshotRequested_t) and this feature always answers with a viewport capture. It gives the key back to Steam
 * when the feature shuts down.
 *
 * Location and tagged users are applied to the next screenshot Steam reports as ready (also to keys pressed with hook
 * mode off). The location stays until you change it; tagged users are used once.
 */
UCLASS()
class SANDWICHSTEAMSCREENSHOTS_API USteamScreenshotsSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Screenshots subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamScreenshotsSubsystem* Get(const UObject* WorldContext);

	USteamScreenshotsSubsystem();
	virtual ~USteamScreenshotsSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** Asks Steam to take a screenshot. With hook mode on this ends up as a viewport capture. */
	FSteamResult TriggerScreenshot();

	/** Captures the game viewport (without UI overlays of the OS or Steam) and hands it to Steam. One capture at a time. */
	FSteamResult CaptureViewportToSteam();

	/** Location text (for example a level name) attached to screenshots. Empty clears it. */
	void SetLocation(const FString& Location);

	/** Tags a user (at most 32) on the next screenshot. */
	FSteamResult TagUser(FSteamId User);

	/** True while the game owns the screenshot key. */
	bool IsHooked() const;

	/** True while a viewport capture is in progress. */
	bool IsCapturePending() const { return bCapturePending; }

	const FString& GetLocation() const { return CurrentLocation; }
	int32 GetTaggedUserCount() const { return TaggedUsers.Num(); }

	/** Screenshots handed to Steam by viewport capture / reported ready, since the feature became active. */
	int32 GetWrittenCount() const { return WrittenCount; }
	int32 GetReadyCount() const { return ReadyCount; }

	/** Called when Steam added a screenshot to the library. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Screenshots", meta = (ToolTip = "Called when Steam finished adding a screenshot to the library."))
	FOnSteamScreenshotReady OnScreenshotReady;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Screenshot.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamScreenshotsBackend;

	/** Game thread, called through the dispatcher by the backend. */
	void HandleScreenshotRequested();
	void HandleScreenshotReady(uint32 Handle, int32 NativeResult);

	/** UGameViewportClient::OnScreenshotCaptured target (game thread). */
	void HandleViewportCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap);

	/** Game thread. The converted image arrives from the worker task. */
	void HandleConverted(uint32 Id, TArray<uint8>&& Rgb, int32 Width, int32 Height);

	void EndCapture();

	TSharedPtr<FSteamScreenshotsBackend> Backend;
	TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;

	FString CurrentLocation;
	TArray<FSteamId> TaggedUsers;

	FDelegateHandle ViewportHandle;
	FTSTicker::FDelegateHandle CaptureTimeout;
	bool bCapturePending = false;

	/** FPlatformTime::Seconds() when the capture in flight was requested (for the timing log). */
	double CaptureStartSeconds = 0.0;

	/** Identifies the capture in flight, so a result that arrives after a timeout or shutdown is dropped. */
	uint32 CaptureId = 0;

	int32 WrittenCount = 0;
	int32 ReadyCount = 0;
};
