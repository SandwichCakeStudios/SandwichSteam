// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Bookkeeping for a bounded least-recently-used cache. It tracks keys only, never values, so the owner can keep
 * its values wherever they need to live (for example in a GC-visible map of textures).
 *
 * Touch() is O(1). Evict() scans once per evicted key, which is fine for the small capacities used here (tens to hundreds).
 * Not thread safe.
 */
template <typename KeyType>
class TSteamLruIndex
{
public:
	explicit TSteamLruIndex(int32 InCapacity = 64)
		: Capacity(FMath::Max(1, InCapacity))
	{
	}

	/** Sets the maximum number of keys (minimum 1). Call Evict() afterwards to drop the excess. */
	void SetCapacity(int32 InCapacity) { Capacity = FMath::Max(1, InCapacity); }

	int32 GetCapacity() const { return Capacity; }
	int32 Num() const { return Stamps.Num(); }
	bool Contains(const KeyType& Key) const { return Stamps.Contains(Key); }

	/** Marks Key as the most recently used and adds it when missing. */
	void Touch(const KeyType& Key) { Stamps.FindOrAdd(Key) = ++Counter; }

	/** Forgets Key. Returns true when it was tracked. */
	bool Remove(const KeyType& Key) { return Stamps.Remove(Key) > 0; }

	void Reset() { Stamps.Reset(); }

	/** Removes the least recently used keys until Num() <= capacity and appends them to OutEvicted (oldest first). */
	void Evict(TArray<KeyType>& OutEvicted)
	{
		while (Stamps.Num() > Capacity)
		{
			const KeyType* Oldest = nullptr;
			uint64 OldestStamp = MAX_uint64;
			for (const TPair<KeyType, uint64>& Pair : Stamps)
			{
				if (Pair.Value < OldestStamp)
				{
					OldestStamp = Pair.Value;
					Oldest = &Pair.Key;
				}
			}

			if (!Oldest)
			{
				break;
			}

			KeyType OldestKey = *Oldest;
			Stamps.Remove(OldestKey);
			OutEvicted.Add(MoveTemp(OldestKey));
		}
	}

private:
	TMap<KeyType, uint64> Stamps;
	uint64 Counter = 0;
	int32 Capacity;
};
