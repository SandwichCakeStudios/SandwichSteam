// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceRateLimiter.h"

namespace SandwichSteam::Voice
{
	void FRateLimiter::Configure(float PacketsPerSecond, float BytesPerSecond)
	{
		PacketRate = FMath::Max(PacketsPerSecond, 1.0f);
		ByteRate = FMath::Max(BytesPerSecond, 1.0f);
		Reset();
	}

	void FRateLimiter::Reset()
	{
		PacketTokens = PacketRate;
		ByteTokens = ByteRate;
		LastRefill = -1.0;
		OverStart = -1.0;
		LastRefused = -1.0;
		Dropped = 0;
		bFlooding = false;
	}

	void FRateLimiter::Refill(double NowSeconds)
	{
		if (LastRefill < 0.0)
		{
			LastRefill = NowSeconds;
			return;
		}

		const float Elapsed = static_cast<float>(FMath::Max(NowSeconds - LastRefill, 0.0));
		LastRefill = NowSeconds;
		PacketTokens = FMath::Min(PacketTokens + Elapsed * PacketRate, PacketRate);
		ByteTokens = FMath::Min(ByteTokens + Elapsed * ByteRate, ByteRate);
	}

	bool FRateLimiter::TryConsume(int32 Bytes, double NowSeconds)
	{
		Refill(NowSeconds);

		const float Size = static_cast<float>(FMath::Max(Bytes, 0));
		if (PacketTokens >= 1.0f && ByteTokens >= Size)
		{
			PacketTokens -= 1.0f;
			ByteTokens -= Size;
			return true;
		}

		++Dropped;

		// A refusal after a quiet pause starts a new streak; a streak that lasts long enough is a flood.
		if (OverStart < 0.0 || NowSeconds - LastRefused > QuietResetSeconds)
		{
			OverStart = NowSeconds;
		}
		LastRefused = NowSeconds;
		if (NowSeconds - OverStart >= FloodSeconds)
		{
			bFlooding = true;
		}
		return false;
	}
}
