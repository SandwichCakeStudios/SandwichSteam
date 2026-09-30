// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceRules.h"

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
}
