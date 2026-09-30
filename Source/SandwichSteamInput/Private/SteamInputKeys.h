// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamInputRules.h"

class USteamAppDefinition;

namespace SandwichSteam::Input
{
	/**
	 * Registers the keys of every action in the definition with the engine (EKeys), in the menu category "Steam Input":
	 * SteamInput_<Action> for a button, SteamInput_<Action> for a trigger (1D axis), SteamInput_<Action>_X / _Y and the 2D pair SteamInput_<Action> for a stick.
	 * Enhanced Input mapping contexts pick them from the key list like any gamepad key. Safe to call again; existing keys are left alone.
	 * Returns the number of keys that were new. Keys cannot be unregistered, so a renamed action leaves its old key behind until the editor restarts.
	 */
	int32 RegisterKeys(const USteamAppDefinition& Definition);

	/** Sends the change to Slate as a gamepad key event of the controller's local player. Game thread. */
	void EmitEvent(const FActionEvent& Event);
}
