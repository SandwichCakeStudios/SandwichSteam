// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamResult.h"
#include "Data/SteamAppDefinition.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "SteamInputRules.h"
#include "SteamInputTypes.h"
#include "SteamInputSubsystem.generated.h"

class FSteamGlyphCache;
class FSteamInputBackend;
class ULocalPlayer;
class UInputMappingContext;

/**
 * Steam Input: Steam Deck, Steam Controller, PlayStation, Switch and Xbox controllers through Steam's configurator, delivered to Enhanced Input.
 * Client only.
 *
 * The actions of the Steam App Definition (Input Sets) become keys named SteamInput_<Action> that you map in Enhanced Input mapping contexts like any
 * gamepad key, so triggers, modifiers and remapping keep working. Each action set can bring its own mapping context: switching the set (by gameplay tag)
 * changes the actions Steam delivers and swaps the context. Glyphs give the images of the buttons the player bound.
 *
 * Nothing runs without a controller except a check for a newly connected controller every Idle Poll Seconds (Input settings). With a controller the
 * feature runs every frame. The Steam client only delivers controllers of an action file (game_actions_<AppId>.vdf: the partner site, or the Action
 * Manifest test setting); see Documents/Systems/SteamInput.md. Do not enable the engine's SteamController plugin at the same time.
 * All methods run on the game thread.
 */
UCLASS()
class SANDWICHSTEAMINPUT_API USteamInputSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Input subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamInputSubsystem* Get(const UObject* WorldContext);

	USteamInputSubsystem();
	virtual ~USteamInputSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** Switches to an action set (not a layer): Steam delivers its actions and its mapping context replaces the previous one. */
	FSteamResult SetActionSet(FGameplayTag SetTag);
	FGameplayTag GetActionSet() const { return ActiveSet; }

	/** Turns a layer on top of the active set on or off. */
	FSteamResult ActivateLayer(FGameplayTag LayerTag);
	FSteamResult DeactivateLayer(FGameplayTag LayerTag);
	TArray<FGameplayTag> GetActiveLayers() const { return ActiveLayers; }

	/** The same by the Steam set name of the row (as in the action file). The row must still exist in the App Definition: its keys and mapping context come from there. */
	FSteamResult SetActionSetByName(FName SteamSetName);
	FSteamResult ActivateLayerByName(FName SteamSetName);
	FSteamResult DeactivateLayerByName(FName SteamSetName);

	/** Steam set name of the active set, or None. */
	FName GetActionSetName() const;

	/** The connected controllers, in slot order. */
	TArray<FSteamInputController> GetControllers() const;
	bool HasController() const;

	/** Slot of the controller that sent input last (0 when none did). Glyphs default to it. */
	int32 GetLastActiveSlot() const { return LastActiveSlot; }

	/**
	 * The glyph of the button the player bound to an action in the active set (and layers). Slot INDEX_NONE = the controller used last.
	 * Fails when no controller is connected or the action is not bound. The label is Steam's name of the button.
	 */
	FSteamResult GetGlyphForAction(FName Action, FSteamInputGlyph& OutGlyph, int32 Slot = INDEX_NONE);

	/** The same for one of the SteamInput_ keys (for example the key an Input Action is mapped to). */
	FSteamResult GetGlyphForKey(FKey Key, FSteamInputGlyph& OutGlyph, int32 Slot = INDEX_NONE);

	/** Rumble. Speeds 0 to 1; trigger speeds only do something on controllers with trigger motors. Duration 0 = until StopVibration. Slot INDEX_NONE = every controller. */
	FSteamResult TriggerVibration(int32 Slot, float Left, float Right, float LeftTrigger, float RightTrigger, float DurationSeconds);
	FSteamResult StopVibration(int32 Slot = INDEX_NONE);

	/** Light bar colour (PlayStation, Steam Controller). Slot INDEX_NONE = every controller. */
	FSteamResult SetLedColor(int32 Slot, FColor Color);
	FSteamResult ResetLedColor(int32 Slot = INDEX_NONE);

	/** Opens Steam's binding panel for the controller (the player rebinds there). Slot INDEX_NONE = the controller used last. */
	FSteamResult ShowBindingPanel(int32 Slot = INDEX_NONE);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Input", meta = (ToolTip = "Called when a controller is connected or disconnected."))
	FSteamInputControllerChanged OnControllerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Input", meta = (ToolTip = "Called when an action set or layer becomes active or inactive."))
	FSteamInputActionSetChanged OnActionSetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Input", meta = (ToolTip = "Called when the button glyphs may have changed: a controller of another type, another action set or layer. Ask for the glyphs again."))
	FSteamInputGlyphsChanged OnGlyphsChanged;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Input.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	/** An action of the active set / layers the frame reads. */
	struct FActiveAction
	{
		FName Name;
		ESteamInputActionKind Kind = ESteamInputActionKind::Button;
		uint64 Handle = 0;
	};

	/** What Steam answered for an action name. */
	struct FResolvedAction
	{
		ESteamInputActionKind Kind = ESteamInputActionKind::Button;
		uint64 Handle = 0;
	};

	/** A mapping context this feature added to a local player (only these are removed again). */
	struct FAddedContext
	{
		TWeakObjectPtr<const UInputMappingContext> Context;
		TWeakObjectPtr<ULocalPlayer> Player;
	};

	bool TickInput(float DeltaTime);
	/** Runs every frame while a controller is connected, else every Idle Poll Seconds. True when the ticker was replaced. */
	bool SetTickRate(bool bFast);

	/** Looks up the Steam handles of every set and action of the definition. Names Steam does not know stay 0 and are retried on a new connection. */
	void ResolveHandles();

	void RebuildActiveActions();
	void ApplySetsToController(uint64 Handle) const;

	void HandleControllersChanged(const TArray<int32>& Added, const TArray<int32>& Removed);
	void EmitEvents(const TArray<SandwichSteam::Input::FActionEvent>& Events);

	/** Makes the local player's mapping contexts match the active set and layers. */
	void SyncContexts();
	void RemoveAllContexts();
	void HandlePostLoadMap(UWorld* World);

	FSteamResult ResolveSlot(int32 Slot, uint64& OutHandle, int32& OutSlot) const;
	uint64 FindSetHandle(const FGameplayTag& Tag) const;
	FSteamResult ResolveSetName(FName SteamSetName, FGameplayTag& OutTag) const;
	FSteamResult MakeGlyph(FName Action, int32 Slot, FSteamInputGlyph& OutGlyph);

	TSharedPtr<FSteamInputBackend> Backend;
	TUniquePtr<SandwichSteam::Input::FSlotTable> Slots;
	TUniquePtr<SandwichSteam::Input::FStateTracker> Tracker;
	TSharedPtr<FSteamGlyphCache> Glyphs;

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	FTSTicker::FDelegateHandle TickHandle;
	bool bFastTick = false;
	FDelegateHandle PostLoadMapHandle;

	// Handles Steam gave for the names of the definition.
	TMap<FGameplayTag, uint64> SetHandles;
	TMap<FName, FResolvedAction> ResolvedActions;
	int32 UnresolvedNames = 0;
	bool bWarnedUnresolved = false;

	FGameplayTag ActiveSet;
	TArray<FGameplayTag> ActiveLayers;
	TArray<FActiveAction> ActiveActions;

	/** The controllers as last reported, by slot (for the disconnect event and the dump). */
	TMap<int32, FSteamInputController> Known;
	double LastTypeCheck = 0.0;
	int32 LastActiveSlot = 0;

	/** Slot to the time (platform seconds) a timed vibration ends. */
	TMap<int32, double> VibrationEnd;

	TMap<FGameplayTag, FAddedContext> AddedContexts;
	bool bContextsDirty = false;

	// Reused every frame, so a running controller does not allocate.
	TArray<uint64> ConnectedBuffer;
	TArray<SandwichSteam::Input::FActionSample> SampleBuffer;
	TArray<SandwichSteam::Input::FActionEvent> EventBuffer;

	// Counters for the dump.
	int64 FramesRun = 0;
	int64 EventsEmitted = 0;
};
