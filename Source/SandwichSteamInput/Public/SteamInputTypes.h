// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SteamInputTypes.generated.h"

class UTexture2D;

/** What kind of controller Steam Input reports. */
UENUM(BlueprintType)
enum class ESteamInputControllerType : uint8
{
	Unknown,
	SteamController,
	/** The built-in controls of a Steam Deck. */
	SteamDeck,
	Xbox360,
	XboxOne,
	PlayStation3,
	PlayStation4,
	PlayStation5,
	SwitchPro,
	SwitchJoyCon,
	GenericGamepad,
	MobileTouch,
	Other
};

/** Size of a controller button glyph image. */
UENUM(BlueprintType)
enum class ESteamGlyphSize : uint8
{
	Small,
	Medium,
	Large
};

/** One controller Steam Input knows about. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMINPUT_API FSteamInputController
{
	GENERATED_BODY()

	/** Position of the controller (0 = first). Slot 0 drives the first local player. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam")
	int32 Slot = INDEX_NONE;

	/** Steam Input handle of the controller (opaque). */
	UPROPERTY(BlueprintReadOnly, Category = "Steam")
	int64 Handle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam")
	ESteamInputControllerType Type = ESteamInputControllerType::Unknown;
};

/** A button glyph for an action: the image Steam draws for the button the player bound, and its name ("Right Trigger"). */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMINPUT_API FSteamInputGlyph
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam")
	TObjectPtr<UTexture2D> Texture = nullptr;

	/** Steam's localized name of the button, for example "Right Trigger". Use it as text when there is no image. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam")
	FText Label;
};

/** A controller was connected or disconnected. Fires on the game thread. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamInputControllerChanged, FSteamInputController, Controller, bool, bConnected);

/** An action set or layer became active or inactive. Fires on the game thread. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamInputActionSetChanged, FGameplayTag, SetTag, bool, bIsLayer, bool, bActive);

/** The buttons the player sees may have changed (controller type, action set or layers changed): ask for the glyphs again. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamInputGlyphsChanged);
