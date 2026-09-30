// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/** Decoded Steam image in BGRA8 order (ready for PF_B8G8R8A8). */
struct FSteamImagePixels
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Bgra;

	bool IsValid() const { return Width > 0 && Height > 0 && Bgra.Num() == Width * Height * 4; }
};

namespace SandwichSteam
{
	/**
	 * Reads a Steam image handle (achievement icon, avatar, ...) through ISteamUtils and converts it to BGRA.
	 * Game thread, only while Steam is Ready. Returns false when the handle is not readable.
	 */
	SANDWICHSTEAM_API bool ReadSteamImage(int32 ImageHandle, FSteamImagePixels& OutPixels);

	/** Creates a transient UI texture from the pixels. Game thread. Returns nullptr on failure. */
	SANDWICHSTEAM_API UTexture2D* CreateTextureFromPixels(const FSteamImagePixels& Pixels);
}
