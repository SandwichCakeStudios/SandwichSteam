// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamScreenshotsSubsystem.h"
#include "Async/Async.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "SteamScreenshotConversion.h"
#include "SteamScreenshotsBackend.h"
#include "SteamScreenshotsSettings.h"
#include "UnrealClient.h"

namespace
{
	/** A viewport capture that never arrives (no viewport, headless) must not block the next one forever. */
	constexpr float CaptureTimeoutSeconds = 5.0f;
}

USteamScreenshotsSubsystem* USteamScreenshotsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamScreenshotsSubsystem>() : nullptr;
}

USteamScreenshotsSubsystem::USteamScreenshotsSubsystem() = default;
USteamScreenshotsSubsystem::~USteamScreenshotsSubsystem() = default;

FGameplayTag USteamScreenshotsSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Screenshots;
}

bool USteamScreenshotsSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	Backend = MakeShared<FSteamScreenshotsBackend>(this, Dispatcher.ToSharedRef());
	WrittenCount = 0;
	ReadyCount = 0;

	const USteamScreenshotsSettings* Settings = USteamScreenshotsSettings::Get();
	if (Settings && Settings->bHookScreenshots)
	{
		Backend->SetHooked(true);
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam screenshots: hook mode on. The game answers the Steam screenshot key with a viewport capture."));
	}
	return true;
#else
	return false;
#endif
}

void USteamScreenshotsSubsystem::ShutdownFeature()
{
	EndCapture();
	++CaptureId; // A result still on its way is dropped.

	// Destroying the backend gives the screenshot key back to Steam.
	Backend.Reset();
	Dispatcher.Reset();
	TaggedUsers.Reset();
}

bool USteamScreenshotsSubsystem::IsHooked() const
{
	return Backend.IsValid() && Backend->IsHooked();
}

FSteamResult USteamScreenshotsSubsystem::TriggerScreenshot()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Backend->TriggerScreenshot();
	return FSteamResult::Success();
}

void USteamScreenshotsSubsystem::SetLocation(const FString& Location)
{
	CurrentLocation = Location;
}

FSteamResult USteamScreenshotsSubsystem::TagUser(FSteamId User)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!User.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "ScreenshotBadUser", "The Steam ID to tag is invalid."));
	}

	if (TaggedUsers.Contains(User))
	{
		return FSteamResult::Success();
	}

	if (TaggedUsers.Num() >= SandwichSteam::Screenshots::MaxTaggedUsers)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_QuotaExceeded, NSLOCTEXT("SandwichSteam", "ScreenshotTooManyUsers", "A screenshot can be tagged with at most 32 users."));
	}

	TaggedUsers.Add(User);
	return FSteamResult::Success();
}

FSteamResult USteamScreenshotsSubsystem::CaptureViewportToSteam()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (bCapturePending)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_RateLimited, NSLOCTEXT("SandwichSteam", "ScreenshotBusy", "A screenshot is already being captured."));
	}

	if (!GEngine || !GEngine->GameViewport)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "ScreenshotNoViewport", "There is no game viewport to capture (dedicated server or commandlet)."));
	}

	bCapturePending = true;
	++CaptureId;
	CaptureStartSeconds = FPlatformTime::Seconds();
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam screenshots: viewport capture requested."));

	// Listen only while this capture is in flight, so the engine's own screenshots are not touched.
	ViewportHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &USteamScreenshotsSubsystem::HandleViewportCaptured);
	CaptureTimeout = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		if (bCapturePending)
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam screenshots: the viewport capture did not arrive within %.0f seconds and was dropped."), CaptureTimeoutSeconds);
			EndCapture();
			++CaptureId;
		}
		return false;
	}), CaptureTimeoutSeconds);

	FScreenshotRequest::RequestScreenshot(/*bInShowUI*/ false);
	return FSteamResult::Success();
}

void USteamScreenshotsSubsystem::EndCapture()
{
	if (ViewportHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ViewportHandle);
		ViewportHandle.Reset();
	}

	FTSTicker::GetCoreTicker().RemoveTicker(CaptureTimeout);
	CaptureTimeout.Reset();
	bCapturePending = false;
}

void USteamScreenshotsSubsystem::HandleViewportCaptured(int32 Width, int32 Height, const TArray<FColor>& Bitmap)
{
	if (!bCapturePending || !Dispatcher.IsValid())
	{
		return;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam screenshots: the engine delivered the %dx%d bitmap after %.0f ms."), Width, Height, (FPlatformTime::Seconds() - CaptureStartSeconds) * 1000.0);

	// One capture per request: stop listening and the timeout. bCapturePending stays until the image is handed to Steam.
	if (ViewportHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ViewportHandle);
		ViewportHandle.Reset();
	}
	FTSTicker::GetCoreTicker().RemoveTicker(CaptureTimeout);
	CaptureTimeout.Reset();

	if (Width <= 0 || Height <= 0 || Bitmap.Num() != Width * Height)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam screenshots: the captured bitmap has an unexpected size (%dx%d, %d pixels)."), Width, Height, Bitmap.Num());
		bCapturePending = false;
		return;
	}

	// Convert on a worker task (a 4K frame is 25 MB of pixels). The bitmap is copied because the engine reuses it.
	// The result comes back through the dispatcher, which skips it when this subsystem is gone.
	const uint32 Id = CaptureId;
	const TWeakObjectPtr<USteamScreenshotsSubsystem> WeakThis(this);
	const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Queue = Dispatcher;
	Async(EAsyncExecution::ThreadPool, [Pixels = Bitmap, Width, Height, Id, WeakThis, Queue]() mutable
	{
		TArray<uint8> Rgb;
		SandwichSteam::Screenshots::ConvertBgraToRgb(Pixels, Rgb);
		Pixels.Empty();

		Queue->EnqueueFor(WeakThis, [Id, Rgb = MoveTemp(Rgb), Width, Height](USteamScreenshotsSubsystem& Screenshots) mutable
		{
			Screenshots.HandleConverted(Id, MoveTemp(Rgb), Width, Height);
		});
	});
}

void USteamScreenshotsSubsystem::HandleConverted(uint32 Id, TArray<uint8>&& Rgb, int32 Width, int32 Height)
{
	if (Id != CaptureId || !Backend.IsValid())
	{
		return; // Timed out, shut down, or replaced by a newer capture.
	}

	bCapturePending = false;

	const uint32 Handle = Backend->WriteScreenshot(Rgb, Width, Height);
	if (Handle == 0)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam screenshots: Steam refused the %dx%d screenshot."), Width, Height);
		return;
	}

	++WrittenCount;
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam screenshots: handed a %dx%d screenshot to Steam (handle %u) %.0f ms after the request."), Width, Height, Handle, (FPlatformTime::Seconds() - CaptureStartSeconds) * 1000.0);
}

void USteamScreenshotsSubsystem::HandleScreenshotRequested()
{
	if (!Backend.IsValid())
	{
		return;
	}

	// With the hook on, Steam does not capture: always answer, or the key press produces nothing.
	if (bCapturePending)
	{
		return; // The capture in flight answers it.
	}

	const FSteamResult Result = CaptureViewportToSteam();
	if (!Result.IsSuccess())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam screenshots: could not answer the screenshot request: %s"), *Result.Message.ToString());
	}
}

void USteamScreenshotsSubsystem::HandleScreenshotReady(uint32 Handle, int32 NativeResult)
{
	if (!Backend.IsValid())
	{
		return;
	}

	// EResult 1 is k_EResultOK.
	const bool bSuccess = NativeResult == 1;
	if (bSuccess)
	{
		++ReadyCount;
		if (!CurrentLocation.IsEmpty())
		{
			Backend->SetLocation(Handle, CurrentLocation);
		}

		for (const FSteamId& User : TaggedUsers)
		{
			Backend->TagUser(Handle, User);
		}
		TaggedUsers.Reset();
	}
	else
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam screenshots: Steam could not add screenshot %u (result %d)."), Handle, NativeResult);
	}

	OnScreenshotReady.Broadcast(static_cast<int32>(Handle), bSuccess);
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamScreenshotsSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Screenshot: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	Report += FString::Printf(TEXT("  Hook mode: %s, capture in progress: %s\n"), IsHooked() ? TEXT("on") : TEXT("off"), bCapturePending ? TEXT("yes") : TEXT("no"));
	Report += FString::Printf(TEXT("  Location: %s, users tagged for the next screenshot: %d\n"), CurrentLocation.IsEmpty() ? TEXT("(none)") : *CurrentLocation, TaggedUsers.Num());
	Report += FString::Printf(TEXT("  Viewport captures handed to Steam: %d, screenshots reported ready: %d\n"), WrittenCount, ReadyCount);
	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
