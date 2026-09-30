// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamImage.h"
#include "Core/SteamSDK.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace SandwichSteam
{
	bool ReadSteamImage(int32 ImageHandle, FSteamImagePixels& OutPixels)
	{
		OutPixels = FSteamImagePixels();

#if SANDWICHSTEAM_WITH_STEAMWORKS
		ISteamUtils* Utils = SteamUtils();
		if (!Utils || ImageHandle <= 0)
		{
			return false;
		}

		uint32 Width = 0;
		uint32 Height = 0;
		if (!Utils->GetImageSize(ImageHandle, &Width, &Height) || Width == 0 || Height == 0)
		{
			return false;
		}

		const int32 ByteCount = static_cast<int32>(Width * Height * 4);
		OutPixels.Width = static_cast<int32>(Width);
		OutPixels.Height = static_cast<int32>(Height);
		OutPixels.Bgra.SetNumUninitialized(ByteCount);
		if (!Utils->GetImageRGBA(ImageHandle, OutPixels.Bgra.GetData(), ByteCount))
		{
			OutPixels = FSteamImagePixels();
			return false;
		}

		// Steam returns RGBA, the texture format is BGRA.
		uint8* Data = OutPixels.Bgra.GetData();
		for (int32 Index = 0; Index < ByteCount; Index += 4)
		{
			Swap(Data[Index], Data[Index + 2]);
		}
		return true;
#else
		return false;
#endif
	}

	UTexture2D* CreateTextureFromPixels(const FSteamImagePixels& Pixels)
	{
		check(IsInGameThread());

		if (!Pixels.IsValid())
		{
			return nullptr;
		}

		UTexture2D* Texture = UTexture2D::CreateTransient(Pixels.Width, Pixels.Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}

		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return nullptr;
		}

		Texture->SRGB = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->LODGroup = TEXTUREGROUP_UI;

		FTexture2DMipMap& Mip = PlatformData->Mips[0];
		void* Destination = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Destination, Pixels.Bgra.GetData(), Pixels.Bgra.Num());
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}
