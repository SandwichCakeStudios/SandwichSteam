// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/SteamAppDefinition.h"

/** One action of an action set in the generated Steam Input action file. */
struct FSteamInputVdfAction
{
	FString Name;
	ESteamInputActionKind Kind = ESteamInputActionKind::Button;

	/** Steam input mode of a stick / pad (joystick_move, ...). Empty = joystick_move. */
	FString InputMode;

	/** English text of the action in the Steam Input configurator. Empty = the name. */
	FString Title;
};

/** One action set (or layer) of the generated Steam Input action file. */
struct FSteamInputVdfSet
{
	FString Name;

	/** English text of the set in the configurator. Empty = the name. */
	FString Title;

	bool bLayer = false;
	TArray<FSteamInputVdfAction> Actions;
};

/**
 * Pure text generation of the Steam Input action file (game_actions_<AppId>.vdf) from action sets and actions. No file access and no
 * engine state, so it is unit tested with golden strings. Output uses tabs and LF line endings.
 *
 * Sets go under "actions", layers under "action_layers". Actions are grouped by kind: Button, AnalogTrigger and StickPadGyro. The titles are
 * localization tokens (#Set_<Name>, #Action_<Name>) with an "english" block, which is what Steam's configurator shows.
 */
class FSteamInputVdfWriter
{
public:
	/** Localization key of a set title (Set_<Name>) and of an action title (Action_<Name>), without the leading #. */
	static FString GetSetKey(const FString& SetName);
	static FString GetActionKey(const FString& ActionName);

	/** The file for the sets (in the given order). Sets and actions without a name are skipped. Empty when there is nothing to write. */
	static FString Build(const TArray<FSteamInputVdfSet>& Sets);
};
