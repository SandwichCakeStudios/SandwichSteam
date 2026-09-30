// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamLeaderboardTypes.h"

namespace SandwichSteam::Leaderboards
{
	/**
	 * Brings a query into the shape Steam accepts, so equal requests compare equal: Global starts at rank 1 or later,
	 * ranges are ordered and limited to MaxPageSize entries, Friends ignores the range, Users drops invalid ids and is
	 * limited to MaxPageSize users.
	 */
	inline FSteamLeaderboardQuery NormalizeQuery(const FSteamLeaderboardQuery& Query)
	{
		FSteamLeaderboardQuery Result;
		Result.Type = Query.Type;

		switch (Query.Type)
		{
		case ESteamLeaderboardRequestType::Global:
			Result.RangeStart = FMath::Max(1, Query.RangeStart);
			Result.RangeEnd = FMath::Clamp(Query.RangeEnd, Result.RangeStart, Result.RangeStart + MaxPageSize - 1);
			break;

		case ESteamLeaderboardRequestType::AroundUser:
			Result.RangeStart = FMath::Min(Query.RangeStart, Query.RangeEnd);
			Result.RangeEnd = FMath::Clamp(FMath::Max(Query.RangeStart, Query.RangeEnd), Result.RangeStart, Result.RangeStart + MaxPageSize - 1);
			break;

		case ESteamLeaderboardRequestType::Friends:
			Result.RangeStart = 0;
			Result.RangeEnd = 0;
			break;

		case ESteamLeaderboardRequestType::Users:
			Result.RangeStart = 0;
			Result.RangeEnd = 0;
			for (const FSteamId& User : Query.Users)
			{
				if (User.IsValid() && Result.Users.Num() < MaxPageSize)
				{
					Result.Users.Add(User);
				}
			}
			break;
		}

		return Result;
	}
}

/**
 * Identity of a download request. Two requests with an equal key ask Steam for the same thing, so they share one call.
 * Build it from a normalized query (SandwichSteam::Leaderboards::NormalizeQuery).
 */
struct FSteamRequestKey
{
	FName Leaderboard;
	ESteamLeaderboardRequestType Type = ESteamLeaderboardRequestType::Global;
	int32 RangeStart = 0;
	int32 RangeEnd = 0;
	TArray<int64> Users;

	static FSteamRequestKey ForDownload(FName InLeaderboard, const FSteamLeaderboardQuery& NormalizedQuery)
	{
		FSteamRequestKey Key;
		Key.Leaderboard = InLeaderboard;
		Key.Type = NormalizedQuery.Type;
		Key.RangeStart = NormalizedQuery.RangeStart;
		Key.RangeEnd = NormalizedQuery.RangeEnd;
		Key.Users.Reserve(NormalizedQuery.Users.Num());
		for (const FSteamId& User : NormalizedQuery.Users)
		{
			Key.Users.Add(User.Value);
		}
		return Key;
	}

	friend bool operator==(const FSteamRequestKey& A, const FSteamRequestKey& B)
	{
		return A.Leaderboard == B.Leaderboard && A.Type == B.Type && A.RangeStart == B.RangeStart && A.RangeEnd == B.RangeEnd && A.Users == B.Users;
	}

	friend uint32 GetTypeHash(const FSteamRequestKey& Key)
	{
		uint32 Hash = HashCombine(GetTypeHash(Key.Leaderboard), ::GetTypeHash(static_cast<uint8>(Key.Type)));
		Hash = HashCombine(Hash, ::GetTypeHash(Key.RangeStart));
		Hash = HashCombine(Hash, ::GetTypeHash(Key.RangeEnd));
		for (const int64 User : Key.Users)
		{
			Hash = HashCombine(Hash, ::GetTypeHash(User));
		}
		return Hash;
	}
};

/**
 * Lets identical in-flight requests share one Steam call. The first Join() of a key returns true: the caller starts the
 * request. Later Join()s of the same key only add their waiter. When the request finishes, Take() hands every waiter
 * back (and forgets the key, so a new request can start). Game thread only, no Steamworks types.
 */
template <typename TKey, typename TWaiter>
class TSteamRequestCoalescer
{
public:
	/** Adds the waiter. Returns true when nothing identical was in flight, i.e. the caller must start the request. */
	bool Join(const TKey& Key, TWaiter Waiter)
	{
		if (TArray<TWaiter>* Existing = InFlight.Find(Key))
		{
			Existing->Add(MoveTemp(Waiter));
			return false;
		}

		InFlight.Add(Key).Add(MoveTemp(Waiter));
		return true;
	}

	/** Removes the key and returns its waiters in join order. Empty when the key was not in flight. */
	TArray<TWaiter> Take(const TKey& Key)
	{
		TArray<TWaiter> Waiters;
		InFlight.RemoveAndCopyValue(Key, Waiters);
		return Waiters;
	}

	bool IsInFlight(const TKey& Key) const { return InFlight.Contains(Key); }

	/** Number of distinct requests in flight. */
	int32 Num() const { return InFlight.Num(); }

	/** Drops every waiter without calling it (feature shutdown). */
	void Reset() { InFlight.Reset(); }

private:
	TMap<TKey, TArray<TWaiter>> InFlight;
};
