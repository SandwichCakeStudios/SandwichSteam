// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamUtilityTypes.generated.h"

/** Whether the on-screen text input hides what is typed. */
UENUM(BlueprintType)
enum class ESteamGamepadTextInputMode : uint8
{
	Normal,
	Password
};

/** Single or multi line on-screen text input. */
UENUM(BlueprintType)
enum class ESteamGamepadTextLineMode : uint8
{
	SingleLine,
	MultiLine
};

/** Layout of the floating (Steam Deck) on-screen keyboard. */
UENUM(BlueprintType)
enum class ESteamFloatingKeyboardMode : uint8
{
	SingleLine,
	MultiLine,
	Email,
	Numeric
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamFloatingKeyboardDismissed);

/** C++ completion of the gamepad text input. bSubmitted is false when the user cancelled. Runs on the game thread. */
DECLARE_DELEGATE_TwoParams(FSteamTextInputDelegate, bool /*bSubmitted*/, const FString& /*Text*/);
