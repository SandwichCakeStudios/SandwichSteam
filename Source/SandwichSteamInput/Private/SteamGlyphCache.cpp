// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamGlyphCache.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

FSteamGlyphCache::FSteamGlyphCache(int32 InCapacity)
	: Lru(InCapacity)
{
}

UTexture2D* FSteamGlyphCache::Find(const FSteamGlyphKey& Key)
{
	if (TObjectPtr<UTexture2D>* Found = Textures.Find(Key))
	{
		Lru.Touch(Key);
		return Found->Get();
	}
	return nullptr;
}

UTexture2D* FSteamGlyphCache::Load(const FSteamGlyphKey& Key, const FString& PngPath)
{
	check(IsInGameThread());

	if (PngPath.IsEmpty() || !FPaths::FileExists(PngPath))
	{
		return nullptr;
	}

	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(PngPath);
	if (!Texture)
	{
		return nullptr;
	}

	Textures.Add(Key, Texture);
	Lru.Touch(Key);

	TArray<FSteamGlyphKey> Evicted;
	Lru.Evict(Evicted);
	for (const FSteamGlyphKey& Old : Evicted)
	{
		Textures.Remove(Old);
	}
	return Texture;
}

void FSteamGlyphCache::Reset()
{
	Textures.Reset();
	Lru.Reset();
}

void FSteamGlyphCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Textures);
}
