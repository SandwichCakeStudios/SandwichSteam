// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamScreenshotsBackend.h"
#include "SteamScreenshotsSubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

FSteamScreenshotsBackend::FSteamScreenshotsBackend(USteamScreenshotsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	RequestedCallback.Register(this, &FSteamScreenshotsBackend::OnScreenshotRequested);
	ReadyCallback.Register(this, &FSteamScreenshotsBackend::OnScreenshotReady);
}

FSteamScreenshotsBackend::~FSteamScreenshotsBackend()
{
	SetHooked(false);
}

void FSteamScreenshotsBackend::TriggerScreenshot() const
{
	if (ISteamScreenshots* Screenshots = SteamScreenshots())
	{
		Screenshots->TriggerScreenshot();
	}
}

void FSteamScreenshotsBackend::SetHooked(bool bInHooked)
{
	if (ISteamScreenshots* Screenshots = SteamScreenshots())
	{
		Screenshots->HookScreenshots(bInHooked);
	}
	bHooked = bInHooked;
}

uint32 FSteamScreenshotsBackend::WriteScreenshot(TConstArrayView<uint8> Rgb, int32 Width, int32 Height) const
{
	ISteamScreenshots* Screenshots = SteamScreenshots();
	if (!Screenshots || Width <= 0 || Height <= 0 || Rgb.Num() != Width * Height * 3)
	{
		return 0;
	}

	// The API takes a non-const pointer but does not modify the pixels.
	const ScreenshotHandle Handle = Screenshots->WriteScreenshot(const_cast<uint8*>(Rgb.GetData()), static_cast<uint32>(Rgb.Num()), Width, Height);
	return Handle == INVALID_SCREENSHOT_HANDLE ? 0 : static_cast<uint32>(Handle);
}

void FSteamScreenshotsBackend::SetLocation(uint32 Handle, const FString& Location) const
{
	if (ISteamScreenshots* Screenshots = SteamScreenshots())
	{
		Screenshots->SetLocation(static_cast<ScreenshotHandle>(Handle), TCHAR_TO_UTF8(*Location));
	}
}

void FSteamScreenshotsBackend::TagUser(uint32 Handle, FSteamId User) const
{
	if (ISteamScreenshots* Screenshots = SteamScreenshots())
	{
		Screenshots->TagUser(static_cast<ScreenshotHandle>(Handle), CSteamID(static_cast<uint64>(User.Value)));
	}
}

// Both handlers run on Steam's callback thread. Copy the payload, dispatch, return.

void FSteamScreenshotsBackend::OnScreenshotRequested(ScreenshotRequested_t* /*Payload*/)
{
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [](USteamScreenshotsSubsystem& Screenshots)
	{
		Screenshots.HandleScreenshotRequested();
	});
}

void FSteamScreenshotsBackend::OnScreenshotReady(ScreenshotReady_t* Payload)
{
	const uint32 Handle = static_cast<uint32>(Payload->m_hLocal);
	const int32 NativeResult = static_cast<int32>(Payload->m_eResult);
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [Handle, NativeResult](USteamScreenshotsSubsystem& Screenshots)
	{
		Screenshots.HandleScreenshotReady(Handle, NativeResult);
	});
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

FSteamScreenshotsBackend::FSteamScreenshotsBackend(USteamScreenshotsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

FSteamScreenshotsBackend::~FSteamScreenshotsBackend() {}
void FSteamScreenshotsBackend::TriggerScreenshot() const {}
void FSteamScreenshotsBackend::SetHooked(bool) {}
uint32 FSteamScreenshotsBackend::WriteScreenshot(TConstArrayView<uint8>, int32, int32) const { return 0; }
void FSteamScreenshotsBackend::SetLocation(uint32, const FString&) const {}
void FSteamScreenshotsBackend::TagUser(uint32, FSteamId) const {}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
