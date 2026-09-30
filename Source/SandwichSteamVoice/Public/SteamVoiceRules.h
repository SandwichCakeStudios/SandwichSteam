// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Pure voice bookkeeping (no engine or Steam access), so it can be unit tested. */
namespace SandwichSteam::Voice
{
	/** One change reported by FTalkingTracker::Update. */
	struct FTalkingChange
	{
		int64 Player = 0;
		bool bTalking = false;
	};

	/**
	 * Turns "who talks right now" snapshots into started / stopped events.
	 * A player who is talking and disappears from the present set (left the game) gets a stopped event, so a talking indicator never sticks.
	 */
	class FTalkingTracker
	{
	public:
		/**
		 * NowTalking: players whose voice is active in this snapshot. Present: everybody who is in the game now.
		 * Talking players that are not present are treated as not talking. Changes are appended to OutChanges (stops first, then starts).
		 */
		void Update(const TSet<int64>& NowTalking, const TSet<int64>& Present, TArray<FTalkingChange>& OutChanges);

		bool IsTalking(int64 Player) const { return Talking.Contains(Player); }
		const TSet<int64>& GetTalking() const { return Talking; }

		/** Reports every talking player as stopped and forgets them (voice was disabled). */
		void Clear(TArray<FTalkingChange>& OutChanges);

	private:
		TSet<int64> Talking;
	};

	/**
	 * Who is muted and whether the backend already knows it.
	 * A mute has two sources: the game (Mute Player) and the Steam block list (auto). A player is muted while either holds.
	 * The backend can only mute a talker that is registered, which happens when the player joins the game, so the wanted state is kept
	 * here and applied whenever a player is present and the backend has not confirmed it yet (NeedsApply / MarkApplied).
	 */
	class FMuteBook
	{
	public:
		/** Returns true when the muted state of the player changed. */
		bool SetUserMuted(int64 Player, bool bMuted, bool bSystemWide);
		bool SetAutoMuted(int64 Player, bool bMuted);

		bool IsMuted(int64 Player) const { return User.Contains(Player) || Auto.Contains(Player); }
		bool IsUserMuted(int64 Player) const { return User.Contains(Player); }
		bool IsAutoMuted(int64 Player) const { return Auto.Contains(Player); }
		bool IsSystemWide(int64 Player) const { return SystemWide.Contains(Player); }

		/** True when the wanted state differs from what the backend last confirmed (an unknown player counts as unmuted there). */
		bool NeedsApply(int64 Player) const;

		/** The backend accepted the wanted state. */
		void MarkApplied(int64 Player);

		/** A new session registers its talkers again: nothing is applied any more. Mutes themselves are kept. */
		void ForgetApplied() { Applied.Reset(); }

		/** Every player with a mute (either source). */
		TArray<int64> GetMutedPlayers() const;

		int32 NumUserMuted() const { return User.Num(); }
		int32 NumAutoMuted() const { return Auto.Num(); }

		void Reset();

	private:
		TSet<int64> User;
		TSet<int64> Auto;
		TSet<int64> SystemWide;
		TSet<int64> Applied;
	};
}
