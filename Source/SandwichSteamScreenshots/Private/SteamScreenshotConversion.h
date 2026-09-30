// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace SandwichSteam::Screenshots
{
	/** Most users one screenshot can be tagged with (Steam's k_nScreenshotMaxTaggedUsers). */
	constexpr int32 MaxTaggedUsers = 32;

	/**
	 * Converts the viewport bitmap (FColor, B G R A in memory) into the tightly packed R G B bytes that
	 * ISteamScreenshots::WriteScreenshot takes: 3 bytes per pixel, alpha dropped. Pure and thread safe, so it can run on a worker task.
	 */
	inline void ConvertBgraToRgb(TConstArrayView<FColor> Pixels, TArray<uint8>& OutRgb)
	{
		OutRgb.SetNumUninitialized(Pixels.Num() * 3);
		uint8* Out = OutRgb.GetData();
		for (const FColor& Pixel : Pixels)
		{
			*Out++ = Pixel.R;
			*Out++ = Pixel.G;
			*Out++ = Pixel.B;
		}
	}
}
