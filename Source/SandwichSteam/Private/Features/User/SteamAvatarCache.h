// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamLruIndex.h"
#include "Features/User/SteamUserTypes.h"
#include "UObject/GCObject.h"

class UTexture2D;

/** Outcome of asking Steam for an avatar image. */
enum class ESteamAvatarFetch : uint8
{
	/** Pixels were returned. */
	Ready,
	/** Steam is still downloading it. Wait for AvatarImageLoaded_t / PersonaStateChange_t. */
	Loading,
	/** The user has no avatar (or Steam could not provide it). */
	None
};

/** Decoded avatar image in BGRA8 order (ready for PF_B8G8R8A8). */
struct FSteamAvatarPixels
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<uint8> Bgra;

	bool IsValid() const { return Width > 0 && Height > 0 && Bgra.Num() == Width * Height * 4; }
};

struct FSteamAvatarKey
{
	FSteamId UserId;
	ESteamAvatarSize Size = ESteamAvatarSize::Medium;

	FSteamAvatarKey() = default;
	FSteamAvatarKey(FSteamId InUserId, ESteamAvatarSize InSize)
		: UserId(InUserId)
		, Size(InSize)
	{
	}

	friend bool operator==(const FSteamAvatarKey& A, const FSteamAvatarKey& B) { return A.UserId == B.UserId && A.Size == B.Size; }
	friend uint32 GetTypeHash(const FSteamAvatarKey& Key) { return HashCombine(GetTypeHash(Key.UserId), static_cast<uint32>(Key.Size)); }
};

/**
 * Bounded LRU cache of avatar textures. Game thread only.
 * The textures are kept alive through FGCObject, eviction order comes from TSteamLruIndex (pure, unit tested).
 */
class FSteamAvatarCache : public FGCObject
{
public:
	explicit FSteamAvatarCache(int32 InCapacity);

	/** Cached texture (marks it most recently used) or nullptr. */
	UTexture2D* Find(const FSteamAvatarKey& Key);

	/** Creates a transient texture from the pixels and caches it, evicting the oldest entries above capacity. Nullptr on failure. */
	UTexture2D* Add(const FSteamAvatarKey& Key, const FSteamAvatarPixels& Pixels);

	/** Drops every size of one user (the avatar changed). Textures already handed out stay valid while referenced. */
	void Invalidate(FSteamId UserId);

	void Reset();
	int32 Num() const { return Textures.Num(); }
	int32 GetCapacity() const { return Lru.GetCapacity(); }

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FSteamAvatarCache"); }
	//~ End FGCObject

private:
	TMap<FSteamAvatarKey, TObjectPtr<UTexture2D>> Textures;
	TSteamLruIndex<FSteamAvatarKey> Lru;
};
