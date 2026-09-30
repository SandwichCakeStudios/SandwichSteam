// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamCallbackDispatcher.h"
#include "Core/SteamSDK.h"
#include "UObject/WeakObjectPtr.h"

class USteamUtilitySubsystem;

/**
 * Raw Steamworks side of the Utility feature (ISteamUtils, ISteamApps).
 * Query methods run on the game thread and must only be called while Steam is Ready.
 * Enum arguments are passed as ints and cast to the SDK enums here, so the subsystem needs no SDK headers.
 */
class FSteamUtilityBackend
{
public:
	FSteamUtilityBackend(USteamUtilitySubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher);

	FSteamUtilityBackend(const FSteamUtilityBackend&) = delete;
	FSteamUtilityBackend& operator=(const FSteamUtilityBackend&) = delete;

	uint32 GetAppId() const;
	FString GetIpCountry() const;
	FString GetSteamUiLanguage() const;
	FString GetGameLanguage() const;
	int64 GetServerRealTime() const;
	int32 GetSecondsSinceAppActive() const;
	bool IsRunningOnSteamDeck() const;
	bool IsBigPictureMode() const;

	/** Modes are ESteamGamepadTextInputMode / ESteamGamepadTextLineMode values. False when Steam cannot show it (not Big Picture / Deck). */
	bool ShowGamepadTextInput(int32 InputMode, int32 LineMode, const FString& Description, uint32 MaxChars, const FString& ExistingText) const;

	/** Text the user entered, valid after the dismissed callback. Length is the byte length reported by Steam. */
	FString GetEnteredGamepadText(uint32 Length) const;

	/** KeyboardMode is an ESteamFloatingKeyboardMode value. */
	bool ShowFloatingGamepadTextInput(int32 KeyboardMode, int32 X, int32 Y, int32 Width, int32 Height) const;
	void DismissFloatingGamepadTextInput() const;

private:
#if SANDWICHSTEAM_WITH_STEAMWORKS
	STEAM_CALLBACK_MANUAL(FSteamUtilityBackend, OnGamepadTextInputDismissed, GamepadTextInputDismissed_t, GamepadTextDismissedCallback);
	STEAM_CALLBACK_MANUAL(FSteamUtilityBackend, OnFloatingTextInputDismissed, FloatingGamepadTextInputDismissed_t, FloatingTextDismissedCallback);
#endif

	TWeakObjectPtr<USteamUtilitySubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};
