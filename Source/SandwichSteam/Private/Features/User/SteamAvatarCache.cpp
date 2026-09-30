// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/User/SteamAvatarCache.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

FSteamAvatarCache::FSteamAvatarCache(int32 InCapacity)
	: Lru(InCapacity)
{
}

UTexture2D* FSteamAvatarCache::Find(const FSteamAvatarKey& Key)
{
	if (const TObjectPtr<UTexture2D>* Found = Textures.Find(Key))
	{
		Lru.Touch(Key);
		return Found->Get();
	}
	return nullptr;
}

UTexture2D* FSteamAvatarCache::Add(const FSteamAvatarKey& Key, const FSteamAvatarPixels& Pixels)
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

	Textures.Add(Key, Texture);
	Lru.Touch(Key);

	TArray<FSteamAvatarKey> Evicted;
	Lru.Evict(Evicted);
	for (const FSteamAvatarKey& EvictedKey : Evicted)
	{
		Textures.Remove(EvictedKey);
	}

	return Texture;
}

void FSteamAvatarCache::Invalidate(FSteamId UserId)
{
	static constexpr ESteamAvatarSize Sizes[] = { ESteamAvatarSize::Small, ESteamAvatarSize::Medium, ESteamAvatarSize::Large };
	for (const ESteamAvatarSize Size : Sizes)
	{
		const FSteamAvatarKey Key(UserId, Size);
		Textures.Remove(Key);
		Lru.Remove(Key);
	}
}

void FSteamAvatarCache::Reset()
{
	Textures.Reset();
	Lru.Reset();
}

void FSteamAvatarCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Textures);
}
