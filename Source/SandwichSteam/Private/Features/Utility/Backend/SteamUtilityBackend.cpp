// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Utility/Backend/SteamUtilityBackend.h"
#include "Features/Utility/SteamUtilitySubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

FSteamUtilityBackend::FSteamUtilityBackend(USteamUtilitySubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	GamepadTextDismissedCallback.Register(this, &FSteamUtilityBackend::OnGamepadTextInputDismissed);
	FloatingTextDismissedCallback.Register(this, &FSteamUtilityBackend::OnFloatingTextInputDismissed);
}

uint32 FSteamUtilityBackend::GetAppId() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? Utils->GetAppID() : 0;
}

FString FSteamUtilityBackend::GetIpCountry() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? FString(UTF8_TO_TCHAR(Utils->GetIPCountry())) : FString();
}

FString FSteamUtilityBackend::GetSteamUiLanguage() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? FString(UTF8_TO_TCHAR(Utils->GetSteamUILanguage())) : FString();
}

FString FSteamUtilityBackend::GetGameLanguage() const
{
	ISteamApps* Apps = SteamApps();
	return Apps ? FString(UTF8_TO_TCHAR(Apps->GetCurrentGameLanguage())) : FString();
}

int64 FSteamUtilityBackend::GetServerRealTime() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? static_cast<int64>(Utils->GetServerRealTime()) : 0;
}

int32 FSteamUtilityBackend::GetSecondsSinceAppActive() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? static_cast<int32>(Utils->GetSecondsSinceAppActive()) : 0;
}

bool FSteamUtilityBackend::IsRunningOnSteamDeck() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils && Utils->IsSteamRunningOnSteamDeck();
}

bool FSteamUtilityBackend::IsBigPictureMode() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils && Utils->IsSteamInBigPictureMode();
}

bool FSteamUtilityBackend::ShowGamepadTextInput(int32 InputMode, int32 LineMode, const FString& Description, uint32 MaxChars, const FString& ExistingText) const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils && Utils->ShowGamepadTextInput(
		static_cast<EGamepadTextInputMode>(InputMode),
		static_cast<EGamepadTextInputLineMode>(LineMode),
		TCHAR_TO_UTF8(*Description),
		MaxChars,
		TCHAR_TO_UTF8(*ExistingText));
}

FString FSteamUtilityBackend::GetEnteredGamepadText(uint32 Length) const
{
	ISteamUtils* Utils = SteamUtils();
	if (!Utils || Length == 0)
	{
		return FString();
	}

	TArray<ANSICHAR> Buffer;
	Buffer.SetNumZeroed(static_cast<int32>(Length) + 1);
	if (!Utils->GetEnteredGamepadTextInput(Buffer.GetData(), static_cast<uint32>(Buffer.Num())))
	{
		return FString();
	}
	return FString(UTF8_TO_TCHAR(Buffer.GetData()));
}

bool FSteamUtilityBackend::ShowFloatingGamepadTextInput(int32 KeyboardMode, int32 X, int32 Y, int32 Width, int32 Height) const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils && Utils->ShowFloatingGamepadTextInput(static_cast<EFloatingGamepadTextInputMode>(KeyboardMode), X, Y, Width, Height);
}

void FSteamUtilityBackend::DismissFloatingGamepadTextInput() const
{
	if (ISteamUtils* Utils = SteamUtils())
	{
		Utils->DismissFloatingGamepadTextInput();
	}
}

// The two handlers below run on Steam's callback thread. Copy the payload, dispatch, return.

void FSteamUtilityBackend::OnGamepadTextInputDismissed(GamepadTextInputDismissed_t* Payload)
{
	const bool bSubmitted = Payload->m_bSubmitted;
	const uint32 Length = Payload->m_unSubmittedText;
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [bSubmitted, Length](USteamUtilitySubsystem& Utility)
	{
		Utility.HandleGamepadTextInputDismissed(bSubmitted, Length);
	});
}

void FSteamUtilityBackend::OnFloatingTextInputDismissed(FloatingGamepadTextInputDismissed_t* /*Payload*/)
{
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [](USteamUtilitySubsystem& Utility)
	{
		Utility.HandleFloatingKeyboardDismissed();
	});
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

FSteamUtilityBackend::FSteamUtilityBackend(USteamUtilitySubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

uint32 FSteamUtilityBackend::GetAppId() const { return 0; }
FString FSteamUtilityBackend::GetIpCountry() const { return FString(); }
FString FSteamUtilityBackend::GetSteamUiLanguage() const { return FString(); }
FString FSteamUtilityBackend::GetGameLanguage() const { return FString(); }
int64 FSteamUtilityBackend::GetServerRealTime() const { return 0; }
int32 FSteamUtilityBackend::GetSecondsSinceAppActive() const { return 0; }
bool FSteamUtilityBackend::IsRunningOnSteamDeck() const { return false; }
bool FSteamUtilityBackend::IsBigPictureMode() const { return false; }
bool FSteamUtilityBackend::ShowGamepadTextInput(int32, int32, const FString&, uint32, const FString&) const { return false; }
FString FSteamUtilityBackend::GetEnteredGamepadText(uint32) const { return FString(); }
bool FSteamUtilityBackend::ShowFloatingGamepadTextInput(int32, int32, int32, int32, int32) const { return false; }
void FSteamUtilityBackend::DismissFloatingGamepadTextInput() const {}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
