// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamLruIndex.h"
#include "SteamInputTypes.h"
#include "UObject/GCObject.h"

class UTexture2D;

/** A glyph is the image of one button (an EInputActionOrigin) at one size. */
struct FSteamGlyphKey
{
	int32 Origin = 0;
	ESteamGlyphSize Size = ESteamGlyphSize::Medium;

	FSteamGlyphKey() = default;
	FSteamGlyphKey(int32 InOrigin, ESteamGlyphSize InSize)
		: Origin(InOrigin)
		, Size(InSize)
	{
	}

	friend bool operator==(const FSteamGlyphKey& A, const FSteamGlyphKey& B) { return A.Origin == B.Origin && A.Size == B.Size; }
	friend uint32 GetTypeHash(const FSteamGlyphKey& Key) { return HashCombine(GetTypeHash(Key.Origin), static_cast<uint32>(Key.Size)); }
};

/**
 * Bounded LRU cache of glyph textures. Game thread only. The textures are kept alive through FGCObject, eviction order comes from
 * TSteamLruIndex. The key is the button, not the action, so a rebound action simply asks for another key.
 */
class FSteamGlyphCache : public FGCObject
{
public:
	explicit FSteamGlyphCache(int32 InCapacity);

	/** Cached texture (marks it most recently used) or nullptr. */
	UTexture2D* Find(const FSteamGlyphKey& Key);

	/** Loads the PNG at the path into a texture and caches it. Nullptr when the file cannot be read. */
	UTexture2D* Load(const FSteamGlyphKey& Key, const FString& PngPath);

	void Reset();
	int32 Num() const { return Textures.Num(); }
	int32 GetCapacity() const { return Lru.GetCapacity(); }

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FSteamGlyphCache"); }
	//~ End FGCObject

private:
	TMap<FSteamGlyphKey, TObjectPtr<UTexture2D>> Textures;
	TSteamLruIndex<FSteamGlyphKey> Lru;
};
