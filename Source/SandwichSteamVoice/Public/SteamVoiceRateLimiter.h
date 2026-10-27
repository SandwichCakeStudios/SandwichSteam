// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Per-talker flood protection (pure, time is passed in, so it can be unit tested). */
namespace SandwichSteam::Voice
{
	/** Two token buckets (packets and bytes), each holding one second of its rate. */
	class FRateLimiter
	{
	public:
		/** Limits per second. Values below 1 are raised to 1. */
		void Configure(float PacketsPerSecond, float BytesPerSecond);

		/** True when the packet is allowed (and takes its tokens). A refused packet takes nothing. */
		bool TryConsume(int32 Bytes, double NowSeconds);

		/**
		 * True once the talker was over the limit for FloodSeconds in a row: refused packets kept coming with no pause longer than
		 * QuietResetSeconds between them. Stays true until Reset (the caller mutes the talker for the session).
		 */
		bool IsFlooding() const { return bFlooding; }

		uint64 GetDropped() const { return Dropped; }

		void Reset();

		static constexpr double FloodSeconds = 5.0;
		static constexpr double QuietResetSeconds = 1.0;

	private:
		void Refill(double NowSeconds);

		float PacketRate = 60.0f;
		float ByteRate = 8192.0f;
		float PacketTokens = 60.0f;
		float ByteTokens = 8192.0f;
		double LastRefill = -1.0;

		double OverStart = -1.0;
		double LastRefused = -1.0;
		uint64 Dropped = 0;
		bool bFlooding = false;
	};
}
