// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "Features/Utility/SteamUtilityTypes.h"
#include "SteamUtilityAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamTextSubmittedDelegate, const FString&, Text);

/**
 * Shows Steam's on-screen text input (Big Picture / Steam Deck). OnTextSubmitted fires with the text;
 * OnFailure (Steam.Error.Cancelled) fires when the user dismissed it, NotSupported when Steam cannot show it.
 * There is no timeout: the input stays open until the user is done.
 */
UCLASS()
class SANDWICHSTEAM_API USteamShowGamepadTextInputAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the entered text. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Utility", meta = (ToolTip = "Called with the text the user entered."))
	FSteamTextSubmittedDelegate OnTextSubmitted;

	/** Shows Steam's on-screen text input. Only works in Big Picture mode or on the Steam Deck. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Utility", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Show Steam Gamepad Text Input", ToolTip = "Shows Steam's on-screen text input (Big Picture or Steam Deck only). OnTextSubmitted fires with the text, OnFailure when the user cancelled or Steam cannot show it."))
	static USteamShowGamepadTextInputAsyncAction* ShowSteamGamepadTextInput(const UObject* WorldContextObject, FText Description, int32 MaxCharacters = 256, FString ExistingText = TEXT(""),
		ESteamGamepadTextInputMode InputMode = ESteamGamepadTextInputMode::Normal, ESteamGamepadTextLineMode LineMode = ESteamGamepadTextLineMode::SingleLine);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FText Description;
	int32 MaxCharacters = 256;
	FString ExistingText;
	ESteamGamepadTextInputMode InputMode = ESteamGamepadTextInputMode::Normal;
	ESteamGamepadTextLineMode LineMode = ESteamGamepadTextLineMode::SingleLine;
	FString SubmittedText;
};
