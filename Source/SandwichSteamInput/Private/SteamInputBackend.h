// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamInputTypes.h"

class UGameInstance;

/**
 * Raw ISteamInput. The header is SDK free (handles are plain 64 bit numbers). Everything runs on the game thread: with Init(true) Steam Input only
 * moves when RunFrame is called, and the feature calls it. No Steam callbacks are used; controllers are found by asking after each RunFrame.
 */
class FSteamInputBackend
{
public:
	explicit FSteamInputBackend(UGameInstance* InGameInstance);
	~FSteamInputBackend();

	FSteamInputBackend(const FSteamInputBackend&) = delete;
	FSteamInputBackend& operator=(const FSteamInputBackend&) = delete;

	/** Init(true) and, when given, the action manifest of the settings. False when Steam Input is not available. */
	bool Init(const FString& ManifestPath);

	/** Shuts Steam Input down (only when Init worked and Steam is still up). */
	void Shutdown();

	bool IsInitialized() const { return bInitialized; }

	/** Lets Steam Input process controller state. Call every frame while a controller is used. */
	void RunFrame();

	/** Handles of the connected controllers (at most 16). */
	void GetConnectedControllers(TArray<uint64>& OutHandles) const;

	ESteamInputControllerType GetControllerType(uint64 Handle) const;

	// ---- Names -> handles (0 = the action file does not have that name) ----
	uint64 GetActionSetHandle(const FString& SetName) const;
	uint64 GetDigitalActionHandle(const FString& ActionName) const;
	uint64 GetAnalogActionHandle(const FString& ActionName) const;

	// ---- Action sets ----
	void ActivateActionSet(uint64 Controller, uint64 SetHandle) const;
	void ActivateLayer(uint64 Controller, uint64 LayerHandle) const;
	void DeactivateLayer(uint64 Controller, uint64 LayerHandle) const;

	// ---- Reading actions ----
	/** False when the action is not active (not in the controller's active set). bDown is only meaningful when true. */
	bool ReadDigital(uint64 Controller, uint64 Action, bool& bDown) const;

	/** False when the action is not active. */
	bool ReadAnalog(uint64 Controller, uint64 Action, float& X, float& Y) const;

	// ---- Glyphs ----
	/** The first button the player bound to the action in the set (an EInputActionOrigin), or 0 when nothing is bound. */
	int32 GetDigitalOrigin(uint64 Controller, uint64 SetHandle, uint64 Action) const;
	int32 GetAnalogOrigin(uint64 Controller, uint64 SetHandle, uint64 Action) const;

	/** Path of the PNG Steam has for the origin, empty when there is none. */
	FString GetGlyphPath(int32 Origin, ESteamGlyphSize Size) const;

	/** Steam's localized name of the origin ("Right Trigger"). */
	FString GetOriginName(int32 Origin) const;

	// ---- Feedback ----
	/** Speeds 0 to 1. Trigger speeds are for controllers with trigger motors (Xbox One). */
	void SetVibration(uint64 Controller, float Left, float Right, float LeftTrigger, float RightTrigger) const;
	void SetLedColor(uint64 Controller, const FColor& Color) const;
	void ResetLedColor(uint64 Controller) const;
	bool ShowBindingPanel(uint64 Controller) const;

private:
	TWeakObjectPtr<UGameInstance> GameInstance;
	bool bInitialized = false;
};
