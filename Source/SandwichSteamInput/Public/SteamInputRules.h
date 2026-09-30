// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/SteamAppDefinition.h"

/** Pure Steam Input bookkeeping (no engine or Steam access), so it can be unit tested. */
namespace SandwichSteam::Input
{
	/**
	 * Which controller sits in which slot. A controller keeps its slot while it stays connected; a new one takes the lowest free slot.
	 * Slot numbers drive the local player (slot 0 = first player).
	 */
	class SANDWICHSTEAMINPUT_API FSlotTable
	{
	public:
		/**
		 * Brings the table in line with the handles Steam reports as connected. Handles beyond MaxSlots are ignored.
		 * OutAdded / OutRemoved receive the slots that gained or lost a controller (removed first).
		 */
		void Update(TConstArrayView<uint64> Connected, int32 MaxSlots, TArray<int32>& OutAdded, TArray<int32>& OutRemoved);

		/** Slot of the handle, or INDEX_NONE. */
		int32 FindSlot(uint64 Handle) const;

		/** Handle in the slot, 0 when the slot is free or out of range. */
		uint64 GetHandle(int32 Slot) const;

		/** Occupied slots in ascending order. */
		TArray<int32> GetOccupiedSlots() const;

		int32 NumConnected() const;

		/** Highest used slot + 1 (slots below it can be free). */
		int32 NumSlots() const { return Slots.Num(); }

		void Reset() { Slots.Reset(); }

	private:
		/** Handle per slot, 0 = free. */
		TArray<uint64> Slots;
	};

	/** What one action reads right now on one controller. */
	struct FActionSample
	{
		FName Action;
		ESteamInputActionKind Kind = ESteamInputActionKind::Button;

		/** False when the action is not part of the controller's active set (Steam says so): it reads as released / zero. */
		bool bActive = false;

		/** Button state. */
		bool bDown = false;

		/** Trigger: X. Stick / pad: X and Y. */
		FVector2f Value = FVector2f::ZeroVector;
	};

	/** A change of one action on one controller. */
	struct FActionEvent
	{
		int32 Slot = INDEX_NONE;
		FName Action;
		ESteamInputActionKind Kind = ESteamInputActionKind::Button;

		/** Button: the new state. Analog: true while the value is not zero. */
		bool bDown = false;

		/** Analog: the new value (zero when released). */
		FVector2f Value = FVector2f::ZeroVector;
	};

	/**
	 * Turns per-frame samples into events. A button reports its two edges, an analog action reports a new value only when it changed
	 * (and always the return to zero). An action that is missing from a frame, or inactive, or whose controller left, is released.
	 */
	class SANDWICHSTEAMINPUT_API FStateTracker
	{
	public:
		/** Values closer than this are the same value. */
		static constexpr float AnalogEpsilon = 0.0005f;

		/** Samples are all the actions that are read this frame on this slot. Events are appended to OutEvents. */
		void Update(int32 Slot, TConstArrayView<FActionSample> Samples, TArray<FActionEvent>& OutEvents);

		/** Releases everything held on the slot (the controller left, or the set changed to one without those actions). */
		void ReleaseSlot(int32 Slot, TArray<FActionEvent>& OutEvents);

		void ReleaseAll(TArray<FActionEvent>& OutEvents);

		/** Number of actions that are held (button down or analog not zero) on any slot. */
		int32 NumHeld() const;

	private:
		struct FHeld
		{
			ESteamInputActionKind Kind = ESteamInputActionKind::Button;
			FVector2f Value = FVector2f::ZeroVector;
		};

		/** Only actions that are held are stored, so an idle controller has no entries. */
		TMap<int32, TMap<FName, FHeld>> Held;
	};
}
