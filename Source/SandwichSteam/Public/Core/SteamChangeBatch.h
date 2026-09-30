// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Collects change flags per key so a burst of Steam callbacks (a login delivers one persona change per friend) turns
 * into one batch that is handled once. Pure and single threaded: use it on the game thread only.
 *
 * Usage: if (Batch.Add(Key, Flags)) { schedule one flush }, and in the flush: for (const auto& Change : Batch.Take()) { ... }.
 */
template <typename KeyType>
class TSteamChangeBatch
{
public:
	/** Merges Flags into the entry of Key. Returns true when the batch was empty before, i.e. the caller has to schedule the flush. */
	bool Add(const KeyType& Key, uint32 Flags)
	{
		const bool bWasEmpty = Entries.IsEmpty();
		if (uint32* Existing = Entries.Find(Key))
		{
			*Existing |= Flags;
		}
		else
		{
			Entries.Add(Key, Flags);
		}
		return bWasEmpty;
	}

	bool IsEmpty() const { return Entries.IsEmpty(); }
	int32 Num() const { return Entries.Num(); }

	/** Returns every entry and empties the batch. */
	TArray<TPair<KeyType, uint32>> Take()
	{
		TArray<TPair<KeyType, uint32>> Result;
		Result.Reserve(Entries.Num());
		for (const TPair<KeyType, uint32>& Entry : Entries)
		{
			Result.Emplace(Entry.Key, Entry.Value);
		}
		Entries.Reset();
		return Result;
	}

	void Reset() { Entries.Reset(); }

private:
	TMap<KeyType, uint32> Entries;
};
