// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamInputTypes.h"
#include "SteamInputLibrary.generated.h"

/**
 * Steam Input for Blueprints. The events (On Controller Changed, On Action Set Changed, On Glyphs Changed) are on the Steam Input Subsystem:
 * Get Game Instance Subsystem > Steam Input Subsystem.
 * Map the SteamInput_<Action> keys in an Enhanced Input mapping context (they are in the key list under "Steam Input").
 * Every function returns a result: Steam.Error.FeatureDisabled when the Input feature is not available.
 */
UCLASS()
class SANDWICHSTEAMINPUT_API USteamInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Switches to a Steam Input action set (by the tag of its row in the Steam App Definition). Steam delivers the actions of that set and its mapping context replaces the previous one."))
	static FSteamResult SetSteamInputActionSet(const UObject* WorldContextObject, FGameplayTag ActionSet);

	UFUNCTION(BlueprintPure, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "The active action set."))
	static FGameplayTag GetSteamInputActionSet(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Turns an action set layer on top of the active set on."))
	static FSteamResult ActivateSteamInputLayer(const UObject* WorldContextObject, FGameplayTag Layer);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Turns an action set layer off."))
	static FSteamResult DeactivateSteamInputLayer(const UObject* WorldContextObject, FGameplayTag Layer);

	UFUNCTION(BlueprintPure, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "The layers that are on."))
	static TArray<FGameplayTag> GetActiveSteamInputLayers(const UObject* WorldContextObject);

	// By Name: the same with the Steam set name (as in the action file) instead of a tag. The set still needs its row in the App Definition.

	UFUNCTION(BlueprintCallable, Category = "Steam|Input|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Set Steam Input Action Set By Name", Keywords = "set switch steam input action set name", ToolTip = "Switches to a Steam Input action set by its Steam set name. The set must be a row of Input Sets in the Steam App Definition (its keys and mapping context come from there)."))
	static FSteamResult SetSteamInputActionSetByName(const UObject* WorldContextObject, FName ActionSetName);

	UFUNCTION(BlueprintPure, Category = "Steam|Input|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Steam Input Action Set Name", Keywords = "get steam input action set name active", ToolTip = "Steam set name of the active action set. None when no set is active."))
	static FName GetSteamInputActionSetName(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Activate Steam Input Layer By Name", Keywords = "activate steam input layer name", ToolTip = "Turns an action set layer on by its Steam set name."))
	static FSteamResult ActivateSteamInputLayerByName(const UObject* WorldContextObject, FName LayerName);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input|By Name", meta = (WorldContext = "WorldContextObject", DisplayName = "Deactivate Steam Input Layer By Name", Keywords = "deactivate steam input layer name", ToolTip = "Turns an action set layer off by its Steam set name."))
	static FSteamResult DeactivateSteamInputLayerByName(const UObject* WorldContextObject, FName LayerName);

	UFUNCTION(BlueprintPure, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "The controllers Steam Input reports, in slot order. Empty while none is connected."))
	static TArray<FSteamInputController> GetSteamInputControllers(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "True while at least one controller is connected through Steam Input."))
	static bool HasSteamInputController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "The glyph (button image and name) of the button the player bound to the action in the active set. Slot -1 = the controller used last. Fails when no controller is connected or the action is not bound."))
	static FSteamResult GetSteamInputGlyphForAction(const UObject* WorldContextObject, FName Action, FSteamInputGlyph& OutGlyph, int32 Slot = -1);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "The glyph of the button behind one of the SteamInput_ keys, for example the key an Input Action is mapped to. Slot -1 = the controller used last."))
	static FSteamResult GetSteamInputGlyphForKey(const UObject* WorldContextObject, FKey Key, FSteamInputGlyph& OutGlyph, int32 Slot = -1);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Rumble. Speeds 0 to 1 (trigger speeds only work on controllers with trigger motors). Duration 0 = until Stop Steam Input Vibration. Slot -1 = every controller."))
	static FSteamResult TriggerSteamInputVibration(const UObject* WorldContextObject, int32 Slot, float Left, float Right, float LeftTrigger, float RightTrigger, float DurationSeconds = 0.3f);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Stops the rumble. Slot -1 = every controller."))
	static FSteamResult StopSteamInputVibration(const UObject* WorldContextObject, int32 Slot = -1);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Sets the light bar colour (PlayStation, Steam Controller). Slot -1 = every controller."))
	static FSteamResult SetSteamInputLedColor(const UObject* WorldContextObject, int32 Slot, FColor Color);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Gives the light bar back to the player's own setting. Slot -1 = every controller."))
	static FSteamResult ResetSteamInputLedColor(const UObject* WorldContextObject, int32 Slot = -1);

	UFUNCTION(BlueprintCallable, Category = "Steam|Input", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens Steam's binding panel for a controller so the player can rebind. Slot -1 = the controller used last."))
	static FSteamResult ShowSteamInputBindingPanel(const UObject* WorldContextObject, int32 Slot = -1);
};
