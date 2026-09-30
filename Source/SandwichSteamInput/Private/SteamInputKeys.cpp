// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamInputKeys.h"
#include "Core/SteamInputRules.h"
#include "Data/SteamAppDefinition.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "SandwichSteamInputKeys"

namespace
{
	const FName MenuCategory(TEXT("SteamInput"));

	void EnsureCategory()
	{
		static bool bRegistered = false;
		if (!bRegistered)
		{
			bRegistered = true;
			EKeys::AddMenuCategoryDisplayInfo(MenuCategory, LOCTEXT("Category", "Steam Input"), TEXT("GraphEditor.KeyEvent_16x"));
		}
	}

	bool AddIfMissing(const FKey& Key, const FText& DisplayName, uint32 Flags)
	{
		if (EKeys::GetKeyDetails(Key).IsValid())
		{
			return false;
		}
		EKeys::AddKey(FKeyDetails(Key, DisplayName, Flags, MenuCategory));
		return true;
	}

	FText MakeDisplayName(FName Action, const TCHAR* Suffix)
	{
		return FText::Format(LOCTEXT("KeyName", "Steam Input {0}{1}"), FText::FromName(Action), FText::FromString(Suffix));
	}
}

int32 SandwichSteam::Input::RegisterKeys(const USteamAppDefinition& Definition)
{
	EnsureCategory();

	int32 Added = 0;
	TSet<FName> Done;
	for (const FSteamInputActionSetDef& Set : Definition.InputSets)
	{
		for (const FSteamInputActionDef& Action : Set.Actions)
		{
			if (Action.SteamName.IsNone() || Done.Contains(Action.SteamName)
				|| CheckName(Action.SteamName.ToString(), /*bIsAction*/ true) != ENameIssue::None)
			{
				continue;
			}
			Done.Add(Action.SteamName);

			const FKey Key(MakeKeyName(Action.SteamName));
			switch (Action.Kind)
			{
			case ESteamInputActionKind::Button:
				Added += AddIfMissing(Key, MakeDisplayName(Action.SteamName, TEXT("")), FKeyDetails::GamepadKey) ? 1 : 0;
				break;

			case ESteamInputActionKind::Trigger:
				Added += AddIfMissing(Key, MakeDisplayName(Action.SteamName, TEXT("")), FKeyDetails::GamepadKey | FKeyDetails::Axis1D) ? 1 : 0;
				break;

			case ESteamInputActionKind::StickPad:
			{
				const FKey KeyX(MakeAxisKeyName(Action.SteamName, /*bYAxis*/ false));
				const FKey KeyY(MakeAxisKeyName(Action.SteamName, /*bYAxis*/ true));
				Added += AddIfMissing(KeyX, MakeDisplayName(Action.SteamName, TEXT(" X")), FKeyDetails::GamepadKey | FKeyDetails::Axis1D) ? 1 : 0;
				Added += AddIfMissing(KeyY, MakeDisplayName(Action.SteamName, TEXT(" Y")), FKeyDetails::GamepadKey | FKeyDetails::Axis1D) ? 1 : 0;
				if (!EKeys::GetKeyDetails(Key).IsValid())
				{
					EKeys::AddPairedKey(FKeyDetails(Key, MakeDisplayName(Action.SteamName, TEXT(" 2D")), FKeyDetails::GamepadKey | FKeyDetails::Axis2D, MenuCategory), KeyX, KeyY);
					++Added;
				}
				break;
			}
			}
		}
	}
	return Added;
}

void SandwichSteam::Input::EmitEvent(const FActionEvent& Event)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	// Slot 0 is the primary player; more controllers count up from there (local multiplayer, [verify] with two controllers).
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	const FPlatformUserId Primary = Mapper.GetPrimaryPlatformUser();
	const FInputDeviceId DefaultDevice = Mapper.GetDefaultInputDevice();
	const FPlatformUserId User = Event.Slot <= 0 ? Primary : FPlatformUserId::CreateFromInternalId(Primary.GetInternalId() + Event.Slot);
	const FInputDeviceId Device = Event.Slot <= 0 ? DefaultDevice : FInputDeviceId::CreateFromInternalId(DefaultDevice.GetId() + Event.Slot);

	FSlateApplication& Slate = FSlateApplication::Get();
	switch (Event.Kind)
	{
	case ESteamInputActionKind::Button:
		if (Event.bDown)
		{
			Slate.OnControllerButtonPressed(MakeKeyName(Event.Action), User, Device, /*IsRepeat*/ false);
		}
		else
		{
			Slate.OnControllerButtonReleased(MakeKeyName(Event.Action), User, Device, /*IsRepeat*/ false);
		}
		break;

	case ESteamInputActionKind::Trigger:
		Slate.OnControllerAnalog(MakeKeyName(Event.Action), User, Device, Event.Value.X);
		break;

	case ESteamInputActionKind::StickPad:
		Slate.OnControllerAnalog(MakeAxisKeyName(Event.Action, /*bYAxis*/ false), User, Device, Event.Value.X);
		Slate.OnControllerAnalog(MakeAxisKeyName(Event.Action, /*bYAxis*/ true), User, Device, Event.Value.Y);
		break;
	}
}

#undef LOCTEXT_NAMESPACE
