// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceJitterBuffer.h"
#include "SteamVoicePacket.h"

namespace SandwichSteam::Voice
{
	FJitterBuffer::FJitterBuffer()
	{
		for (FSlot& Slot : Slots)
		{
			Slot.Payload.Reserve(MaxPacketBytes);
		}
	}

	void FJitterBuffer::ClearSlots()
	{
		for (FSlot& Slot : Slots)
		{
			Slot.bUsed = false;
			Slot.Payload.Reset();
		}
		Queued = 0;
	}

	void FJitterBuffer::Reset()
	{
		ClearSlots();
		bStarted = false;
		NextSequence = 0;
	}

	FJitterBuffer::EPushResult FJitterBuffer::Push(uint16 Sequence, TConstArrayView<uint8> Payload, double NowSeconds, double Stamp)
	{
		++Stats.Received;

		EPushResult Result = EPushResult::Queued;
		if (!bStarted)
		{
			bStarted = true;
			NextSequence = Sequence;
		}

		const int32 Delta = SequenceDelta(Sequence, NextSequence);
		if (Delta < 0)
		{
			++Stats.Late;
			return EPushResult::Late;
		}
		if (Delta >= Capacity)
		{
			++Stats.Resyncs;
			ClearSlots();
			NextSequence = Sequence;
			Result = EPushResult::Resync;
		}

		FSlot& Slot = Slots[Sequence % Capacity];
		if (Slot.bUsed)
		{
			// Inside the window every slot maps to exactly one sequence, so a used slot holds this one.
			++Stats.Duplicate;
			return EPushResult::Duplicate;
		}

		Slot.Payload.Reset();
		Slot.Payload.Append(Payload.GetData(), Payload.Num());
		Slot.Arrival = NowSeconds;
		Slot.Stamp = Stamp;
		Slot.Sequence = Sequence;
		Slot.bUsed = true;
		++Queued;
		return Result;
	}

	FJitterBuffer::EPopResult FJitterBuffer::Pop(double NowSeconds, double MaxWaitSeconds, TConstArrayView<uint8>& OutPayload, double& OutStamp)
	{
		if (Queued == 0)
		{
			return EPopResult::None;
		}

		FSlot& Next = Slots[NextSequence % Capacity];
		if (Next.bUsed)
		{
			Next.bUsed = false;
			--Queued;
			++NextSequence;
			OutPayload = TConstArrayView<uint8>(Next.Payload);
			OutStamp = Next.Stamp;
			return EPopResult::Packet;
		}

		// A gap: later packets are queued. Skip the missing one once the oldest of them waited long enough.
		double Oldest = NowSeconds;
		for (const FSlot& Slot : Slots)
		{
			if (Slot.bUsed)
			{
				Oldest = FMath::Min(Oldest, Slot.Arrival);
			}
		}

		if (NowSeconds - Oldest >= MaxWaitSeconds)
		{
			++Stats.Lost;
			++NextSequence;
			return EPopResult::Lost;
		}
		return EPopResult::None;
	}
}
