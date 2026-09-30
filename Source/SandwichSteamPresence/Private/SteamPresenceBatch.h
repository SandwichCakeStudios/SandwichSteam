// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamPresenceRules.h"

/**
 * The rich presence keys of the local user: what Steam has (Applied) and what the game changed since the last flush (Staged).
 *
 * Pure (no SDK, no engine state) so the debounce is unit tested. Set/Remove/Clear only stage; Flush() compares the wanted
 * state with the applied one and returns just the calls that are really needed, so a status that flips back and forth
 * within a frame, or is set to the value it already has, costs no Steam call. Setting an empty value removes the key.
 * Game thread only.
 */
class FSteamPresenceBatch
{
public:
	/** The Steam calls one flush needs, in this order: ClearRichPresence, removals, then sets. */
	struct FFlush
	{
		bool bClearAll = false;
		TArray<FString> Removes;
		TArray<TPair<FString, FString>> Sets;

		bool IsEmpty() const { return !bClearAll && Removes.IsEmpty() && Sets.IsEmpty(); }
	};

	/** Stages a key. Returns the reason when the key or value is not allowed or Steam's 30 key limit would be exceeded (nothing is staged then). */
	SandwichSteam::Presence::EIssue Set(const FString& Key, const FString& Value);

	/** Stages the removal of a key. */
	SandwichSteam::Presence::EIssue Remove(const FString& Key);

	/** Stages the removal of every key. */
	void Clear();

	/** True when something was staged since the last flush (it may still turn out to need no Steam call). */
	bool HasPending() const { return bClearStaged || !Staged.IsEmpty() || !StagedRemoves.IsEmpty(); }

	/** Applies the staged changes to the model and returns the Steam calls that make Steam match it. */
	FFlush Flush();

	/**
	 * Steam lost the applied keys (the feature was reactivated): stage every applied key again so the next flush
	 * sends them all. Newer staged changes win.
	 */
	void RequeueApplied();

	/** Number of keys that will be set after the next flush. */
	int32 GetDesiredCount() const;

	const TMap<FString, FString>& GetApplied() const { return Applied; }

private:
	TMap<FString, FString> BuildDesired() const;

	TMap<FString, FString> Applied;
	TMap<FString, FString> Staged;
	TSet<FString> StagedRemoves;
	bool bClearStaged = false;
};
