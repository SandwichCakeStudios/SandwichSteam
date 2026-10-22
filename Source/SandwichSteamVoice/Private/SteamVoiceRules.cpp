// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceRules.h"
#include "Algo/BinarySearch.h"
#include "Misc/Crc.h"

namespace SandwichSteam::Voice
{
	void FTalkingTracker::Update(const TSet<int64>& NowTalking, const TSet<int64>& Present, TArray<FTalkingChange>& OutChanges)
	{
		// Stops first: a player who stopped, or who left while talking.
		for (auto It = Talking.CreateIterator(); It; ++It)
		{
			if (!NowTalking.Contains(*It) || !Present.Contains(*It))
			{
				OutChanges.Add({ *It, false });
				It.RemoveCurrent();
			}
		}

		for (const int64 Player : NowTalking)
		{
			if (Present.Contains(Player) && !Talking.Contains(Player))
			{
				Talking.Add(Player);
				OutChanges.Add({ Player, true });
			}
		}
	}

	bool FTalkingTracker::Set(int64 Player, bool bTalking)
	{
		if (bTalking)
		{
			bool bAlreadyTalking = false;
			Talking.Add(Player, &bAlreadyTalking);
			return !bAlreadyTalking;
		}
		return Talking.Remove(Player) > 0;
	}

	void FTalkingTracker::Clear(TArray<FTalkingChange>& OutChanges)
	{
		for (const int64 Player : Talking)
		{
			OutChanges.Add({ Player, false });
		}
		Talking.Reset();
	}

	bool FMuteBook::SetUserMuted(int64 Player, bool bMuted, bool bSystemWide)
	{
		const bool bWasMuted = IsMuted(Player);
		if (bMuted)
		{
			User.Add(Player);
			if (bSystemWide)
			{
				SystemWide.Add(Player);
			}
			else
			{
				SystemWide.Remove(Player);
			}
		}
		else
		{
			User.Remove(Player);
			SystemWide.Remove(Player);
		}
		return IsMuted(Player) != bWasMuted;
	}

	bool FMuteBook::SetAutoMuted(int64 Player, bool bMuted)
	{
		const bool bWasMuted = IsMuted(Player);
		if (bMuted)
		{
			Auto.Add(Player);
		}
		else
		{
			Auto.Remove(Player);
		}
		return IsMuted(Player) != bWasMuted;
	}

	bool FMuteBook::NeedsApply(int64 Player) const
	{
		return Applied.Contains(Player) != IsMuted(Player);
	}

	void FMuteBook::MarkApplied(int64 Player)
	{
		if (IsMuted(Player))
		{
			Applied.Add(Player);
		}
		else
		{
			Applied.Remove(Player);
		}
	}

	TArray<int64> FMuteBook::GetMutedPlayers() const
	{
		TSet<int64> All = User;
		All.Append(Auto);
		return All.Array();
	}

	void FMuteBook::Reset()
	{
		User.Reset();
		Auto.Reset();
		SystemWide.Reset();
		Applied.Reset();
	}

	bool FChannelRule::Allows(int32 Id) const
	{
		return !bRestricted || Algo::BinarySearch(Allowed, Id) != INDEX_NONE;
	}

	void FilterForTalker(const FPlayerRules& Talker, bool bMouth, TConstArrayView<int32> Channels, FVoiceRoute& Out)
	{
		Out.bMouth = false;
		Out.Channels.Reset();
		if (!Talker.bCanTalk || Talker.bRefused)
		{
			return;
		}

		Out.bMouth = bMouth && Talker.bOwnerVoice;
		for (const int32 Id : Channels)
		{
			if (Talker.Transmit.Allows(Id))
			{
				Out.Channels.Add(Id);
			}
		}
	}

	void FilterForListener(const FPlayerRules& Listener, bool bMouthInRange, const FVoiceRoute& Route, FVoiceRoute& Out)
	{
		Out.bMouth = false;
		Out.Channels.Reset();
		if (Listener.bRefused)
		{
			return;
		}

		Out.bMouth = Route.bMouth && bMouthInRange;
		for (const int32 Id : Route.Channels)
		{
			if (Listener.Listen.Allows(Id))
			{
				Out.Channels.Add(Id);
			}
		}
	}

	void IntersectRoute(const FVoiceRoute& Allowed, FVoiceRoute& InOut)
	{
		InOut.bMouth = InOut.bMouth && Allowed.bMouth;
		InOut.Channels.RemoveAll([&Allowed](int32 Id) { return !Allowed.Channels.Contains(Id); });
		InOut.Channels.Sort();
		for (int32 Index = InOut.Channels.Num() - 1; Index > 0; --Index)
		{
			if (InOut.Channels[Index] == InOut.Channels[Index - 1])
			{
				InOut.Channels.RemoveAt(Index, 1, EAllowShrinking::No);
			}
		}
	}

	float ResolveOwnerVoiceDistance(float Global, float TalkerOverride)
	{
		if (TalkerOverride > 0.0f)
		{
			return TalkerOverride;
		}
		if (TalkerOverride < 0.0f)
		{
			return 0.0f;
		}
		return FMath::Max(Global, 0.0f);
	}

	bool UpdateMouthInRange(bool bWasInRange, bool bKnown, double DistanceSquared, float MaxDistance)
	{
		if (MaxDistance <= 0.0f)
		{
			return true;
		}
		if (!bKnown)
		{
			return false;
		}
		const double Limit = bWasInRange ? MaxDistance * (1.0 + OwnerVoiceHysteresis) : MaxDistance;
		return DistanceSquared <= Limit * Limit;
	}

	bool IsMouthAcceptable(bool bKnown, double DistanceSquared, float MaxDistance)
	{
		if (MaxDistance <= 0.0f)
		{
			return true;
		}
		const double Limit = MaxDistance * (1.0 + OwnerVoiceReceiveSlack);
		return bKnown && DistanceSquared <= Limit * Limit;
	}

	uint32 HashChannelMap(const TMap<FGameplayTag, int32>& TagMap)
	{
		TArray<TPair<FString, int32>> Entries;
		Entries.Reserve(TagMap.Num());
		for (const TPair<FGameplayTag, int32>& Pair : TagMap)
		{
			Entries.Emplace(Pair.Key.ToString(), Pair.Value);
		}
		Entries.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Key < B.Key; });

		uint32 Hash = 0;
		for (const TPair<FString, int32>& Entry : Entries)
		{
			Hash = FCrc::StrCrc32(*Entry.Key, Hash);
			Hash = FCrc::MemCrc32(&Entry.Value, sizeof(Entry.Value), Hash);
		}
		return Hash;
	}

	void FindDuplicateIds(TConstArrayView<int64> Ids, TSet<int64>& Out)
	{
		Out.Reset();
		TSet<int64> Seen;
		Seen.Reserve(Ids.Num());
		for (const int64 Id : Ids)
		{
			bool bAlreadySeen = false;
			Seen.Add(Id, &bAlreadySeen);
			if (bAlreadySeen)
			{
				Out.Add(Id);
			}
		}
	}
}
