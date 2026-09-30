// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Features/Utility/SteamUtilityTypes.h"
#include "SteamUtilitySubsystem.generated.h"

class FSteamUtilityBackend;

/**
 * Small Steam facts and helpers: App ID, IP country, languages, server time, Steam Deck / Big Picture flags,
 * on-screen (gamepad) text input and the option to follow the Steam language.
 * Client only. Backend: raw ISteamUtils / ISteamApps.
 */
UCLASS()
class SANDWICHSTEAM_API USteamUtilitySubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Utility subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamUtilitySubsystem* Get(const UObject* WorldContext);

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** App ID Steam runs this game as (the Steam client's view, so 480 while testing with Spacewar). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "App ID Steam runs this game as. 0 when Steam is not active."))
	int32 GetAppId() const;

	/** Two letter country code derived from the user's IP address (e.g. "US"). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "Two letter country code of the user's IP address (for example \"US\"). Empty when unknown."))
	FString GetIpCountry() const;

	/** Language of the Steam client UI as a Steam API language name (e.g. "english"). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "Language of the Steam client UI as a Steam API language name, for example \"english\" or \"schinese\"."))
	FString GetSteamUiLanguage() const;

	/** Language the user selected for this game in Steam (e.g. "german"). */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "Language the user selected for this game in its Steam properties, as a Steam API language name."))
	FString GetCurrentGameLanguage() const;

	/** Current time according to the Steam servers (UTC). Not affected by the local clock. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "Current UTC time according to the Steam servers. Not affected by the local clock."))
	FDateTime GetServerRealTime() const;

	/** Seconds since this game became the active Steam application. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "Seconds since the game was launched or became the active Steam application."))
	int32 GetSecondsSinceAppActive() const;

	/** True when running on a Steam Deck. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "True when the game runs on a Steam Deck."))
	bool IsRunningOnSteamDeck() const;

	/** True when the Steam client is in Big Picture mode. */
	UFUNCTION(BlueprintPure, Category = "Steam|Utility", meta = (ToolTip = "True when the Steam client is in Big Picture mode."))
	bool IsBigPictureMode() const;

	/**
	 * Shows Steam's on-screen text input. Only available in Big Picture mode / on the Steam Deck, otherwise it fails with
	 * Steam.Error.NotSupported. Only one input can be open. OnDismissed runs on the game thread.
	 */
	FSteamResult ShowGamepadTextInput(ESteamGamepadTextInputMode InputMode, ESteamGamepadTextLineMode LineMode, const FText& Description,
		int32 MaxCharacters, const FString& ExistingText, FSteamTextInputDelegate OnDismissed);

	/** Shows the floating on-screen keyboard (Steam Deck) next to the given text field (screen pixels). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utility", meta = (ToolTip = "Shows the floating Steam on-screen keyboard (Steam Deck) next to a text field given in screen pixels. Returns a failed result when it cannot be shown."))
	FSteamResult ShowFloatingGamepadTextInput(ESteamFloatingKeyboardMode Mode, FIntPoint FieldPosition, FIntPoint FieldSize);

	/** Hides the floating on-screen keyboard. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utility", meta = (ToolTip = "Hides the floating Steam on-screen keyboard."))
	void DismissFloatingGamepadTextInput();

	/** Called when the floating on-screen keyboard was dismissed. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Utility", meta = (ToolTip = "Called when the floating Steam on-screen keyboard was dismissed."))
	FOnSteamFloatingKeyboardDismissed OnFloatingGamepadTextInputDismissed;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Utility.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamUtilityBackend;

	/** Game-thread handlers for the raw callbacks (invoked through the dispatcher). */
	void HandleGamepadTextInputDismissed(bool bSubmitted, uint32 SubmittedLength);
	void HandleFloatingKeyboardDismissed();

	void ApplySteamLanguage() const;

	TSharedPtr<FSteamUtilityBackend> Backend;
	FSteamTextInputDelegate PendingTextInput;
};
