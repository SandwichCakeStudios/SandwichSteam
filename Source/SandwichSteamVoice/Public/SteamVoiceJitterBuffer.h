// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Per-talker packet reordering (pure, time is passed in, so it can be unit tested). */
namespace SandwichSteam::Voice
{
	/**
	 * Puts one talker's packets back in sequence order. It only reorders: the playout delay lives in the speaker's buffer.
	 * A fixed ring of slots whose payload storage is reused, so steady use does not allocate.
	 * Sequence numbers wrap around (16 bit).
	 */
	class FJitterBuffer
	{
	public:
		static constexpr int32 Capacity = 32;

		enum class EPushResult : uint8
		{
			Queued,
			/** Older than what was already played or skipped. Dropped. */
			Late,
			/** Already queued. Dropped. */
			Duplicate,
			/** Too far ahead of the expected packet (talker restarted, long loss): everything queued was dropped and the stream restarts here. */
			Resync
		};

		enum class EPopResult : uint8
		{
			/** Nothing to play yet. */
			None,
			Packet,
			/** The expected packet did not come within the wait time and was skipped. */
			Lost
		};

		struct FStats
		{
			uint64 Received = 0;
			uint64 Late = 0;
			uint64 Duplicate = 0;
			uint64 Lost = 0;
			uint64 Resyncs = 0;
		};

		FJitterBuffer();

		/** Stamp is carried with the packet and returned by Pop (capture time for latency measuring, 0 when unknown). */
		EPushResult Push(uint16 Sequence, TConstArrayView<uint8> Payload, double NowSeconds, double Stamp = 0.0);

		/**
		 * Next step of the stream. Packet: OutPayload / OutStamp are set; the view is valid until the next Push.
		 * Lost: the missing packet waited MaxWaitSeconds since a later packet arrived and was skipped. Call until it returns None.
		 */
		EPopResult Pop(double NowSeconds, double MaxWaitSeconds, TConstArrayView<uint8>& OutPayload, double& OutStamp);

		int32 GetQueued() const { return Queued; }
		const FStats& GetStats() const { return Stats; }

		/** Forgets the stream (statistics are kept). */
		void Reset();

	private:
		struct FSlot
		{
			TArray<uint8> Payload;
			double Arrival = 0.0;
			double Stamp = 0.0;
			uint16 Sequence = 0;
			bool bUsed = false;
		};

		void ClearSlots();

		FSlot Slots[Capacity];
		FStats Stats;
		int32 Queued = 0;
		uint16 NextSequence = 0;
		bool bStarted = false;
	};
}
