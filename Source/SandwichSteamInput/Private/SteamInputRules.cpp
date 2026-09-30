// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamInputRules.h"

namespace SandwichSteam::Input
{
	// ---- FSlotTable ----

	void FSlotTable::Update(TConstArrayView<uint64> Connected, int32 MaxSlots, TArray<int32>& OutAdded, TArray<int32>& OutRemoved)
	{
		MaxSlots = FMath::Max(1, MaxSlots);

		// Controllers that left free their slot.
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			if (Slots[Slot] != 0 && !Connected.Contains(Slots[Slot]))
			{
				Slots[Slot] = 0;
				OutRemoved.Add(Slot);
			}
		}

		// New controllers take the lowest free slot.
		for (const uint64 Handle : Connected)
		{
			if (Handle == 0 || Slots.Contains(Handle))
			{
				continue;
			}

			int32 Free = Slots.IndexOfByKey(0ull);
			if (Free == INDEX_NONE)
			{
				if (Slots.Num() >= MaxSlots)
				{
					continue; // More controllers than slots: the extra ones are ignored.
				}
				Free = Slots.Add(0);
			}

			Slots[Free] = Handle;
			OutAdded.Add(Free);
		}

		// A limit that shrank: controllers above it lose their slot.
		for (int32 Slot = MaxSlots; Slot < Slots.Num(); ++Slot)
		{
			if (Slots[Slot] != 0)
			{
				OutRemoved.AddUnique(Slot);
			}
		}
		if (Slots.Num() > MaxSlots)
		{
			Slots.SetNum(MaxSlots);
		}

		// Trailing free slots are not kept.
		while (!Slots.IsEmpty() && Slots.Last() == 0)
		{
			Slots.Pop();
		}
	}

	int32 FSlotTable::FindSlot(uint64 Handle) const
	{
		return Handle == 0 ? INDEX_NONE : Slots.IndexOfByKey(Handle);
	}

	uint64 FSlotTable::GetHandle(int32 Slot) const
	{
		return Slots.IsValidIndex(Slot) ? Slots[Slot] : 0;
	}

	TArray<int32> FSlotTable::GetOccupiedSlots() const
	{
		TArray<int32> Result;
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			if (Slots[Slot] != 0)
			{
				Result.Add(Slot);
			}
		}
		return Result;
	}

	int32 FSlotTable::NumConnected() const
	{
		int32 Count = 0;
		for (const uint64 Handle : Slots)
		{
			Count += Handle != 0 ? 1 : 0;
		}
		return Count;
	}

	// ---- FStateTracker ----

	namespace
	{
		FActionEvent MakeEvent(int32 Slot, FName Action, ESteamInputActionKind Kind, bool bDown, const FVector2f& Value)
		{
			FActionEvent Event;
			Event.Slot = Slot;
			Event.Action = Action;
			Event.Kind = Kind;
			Event.bDown = bDown;
			Event.Value = Value;
			return Event;
		}

		bool IsHeldValue(ESteamInputActionKind Kind, bool bDown, const FVector2f& Value)
		{
			switch (Kind)
			{
			case ESteamInputActionKind::Button:
				return bDown;
			case ESteamInputActionKind::Trigger:
				return Value.X > 0.0f;
			default:
				return Value.X != 0.0f || Value.Y != 0.0f;
			}
		}
	}

	void FStateTracker::Update(int32 Slot, TConstArrayView<FActionSample> Samples, TArray<FActionEvent>& OutEvents)
	{
		TMap<FName, FHeld>& SlotHeld = Held.FindOrAdd(Slot);

		TSet<FName> Seen;
		Seen.Reserve(Samples.Num());

		for (const FActionSample& Sample : Samples)
		{
			Seen.Add(Sample.Action);

			// An action outside the active set reads as released.
			const bool bDown = Sample.bActive && Sample.bDown;
			FVector2f Value = Sample.bActive ? Sample.Value : FVector2f::ZeroVector;
			if (Sample.Kind == ESteamInputActionKind::Trigger)
			{
				Value = FVector2f(FMath::Clamp(Value.X, 0.0f, 1.0f), 0.0f);
			}
			else if (Sample.Kind == ESteamInputActionKind::Button)
			{
				Value = FVector2f::ZeroVector;
			}

			FHeld* Previous = SlotHeld.Find(Sample.Action);
			const bool bHeldNow = IsHeldValue(Sample.Kind, bDown, Value);

			if (Sample.Kind == ESteamInputActionKind::Button)
			{
				if (bHeldNow && !Previous)
				{
					SlotHeld.Add(Sample.Action, FHeld{ Sample.Kind, FVector2f::ZeroVector });
					OutEvents.Add(MakeEvent(Slot, Sample.Action, Sample.Kind, true, FVector2f::ZeroVector));
				}
				else if (!bHeldNow && Previous)
				{
					SlotHeld.Remove(Sample.Action);
					OutEvents.Add(MakeEvent(Slot, Sample.Action, Sample.Kind, false, FVector2f::ZeroVector));
				}
				continue;
			}

			// Analog: a new value is reported when it moved, and the return to zero always is.
			if (bHeldNow)
			{
				if (!Previous)
				{
					SlotHeld.Add(Sample.Action, FHeld{ Sample.Kind, Value });
					OutEvents.Add(MakeEvent(Slot, Sample.Action, Sample.Kind, true, Value));
				}
				else if (!FMath::IsNearlyEqual(Previous->Value.X, Value.X, AnalogEpsilon) || !FMath::IsNearlyEqual(Previous->Value.Y, Value.Y, AnalogEpsilon))
				{
					Previous->Value = Value;
					OutEvents.Add(MakeEvent(Slot, Sample.Action, Sample.Kind, true, Value));
				}
			}
			else if (Previous)
			{
				SlotHeld.Remove(Sample.Action);
				OutEvents.Add(MakeEvent(Slot, Sample.Action, Sample.Kind, false, FVector2f::ZeroVector));
			}
		}

		// Held actions that were not read this frame (the set changed) are released.
		for (auto It = SlotHeld.CreateIterator(); It; ++It)
		{
			if (!Seen.Contains(It.Key()))
			{
				OutEvents.Add(MakeEvent(Slot, It.Key(), It.Value().Kind, false, FVector2f::ZeroVector));
				It.RemoveCurrent();
			}
		}

		if (SlotHeld.IsEmpty())
		{
			Held.Remove(Slot);
		}
	}

	void FStateTracker::ReleaseSlot(int32 Slot, TArray<FActionEvent>& OutEvents)
	{
		if (const TMap<FName, FHeld>* SlotHeld = Held.Find(Slot))
		{
			for (const TPair<FName, FHeld>& Pair : *SlotHeld)
			{
				OutEvents.Add(MakeEvent(Slot, Pair.Key, Pair.Value.Kind, false, FVector2f::ZeroVector));
			}
			Held.Remove(Slot);
		}
	}

	void FStateTracker::ReleaseAll(TArray<FActionEvent>& OutEvents)
	{
		TArray<int32> HeldSlots;
		Held.GetKeys(HeldSlots);
		for (const int32 Slot : HeldSlots)
		{
			ReleaseSlot(Slot, OutEvents);
		}
	}

	int32 FStateTracker::NumHeld() const
	{
		int32 Count = 0;
		for (const TPair<int32, TMap<FName, FHeld>>& Pair : Held)
		{
			Count += Pair.Value.Num();
		}
		return Count;
	}
}
