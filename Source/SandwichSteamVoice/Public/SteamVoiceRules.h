// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SteamVoicePacket.h"

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

		/** One player started or stopped talking (event-driven source). Returns true when that changed the state. */
		bool Set(int64 Player, bool bTalking);

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

	/** Which channel ids a server rule allows (Transmit or Listen), resolved for the voice thread. */
	struct FChannelRule
	{
		/** Off: every channel. On: only Allowed; on with an empty list: none. */
		bool bRestricted = false;
		/** Sorted, unique. */
		TArray<int32> Allowed;

		bool Allows(int32 Id) const;

		bool operator==(const FChannelRule& Other) const { return bRestricted == Other.bRestricted && Allowed == Other.Allowed; }
	};

	/** One player's server rules, resolved for the voice thread (FSteamVoicePlayerRules with channel ids). */
	struct FPlayerRules
	{
		bool bCanTalk = true;
		bool bOwnerVoice = true;
		/** Two players with this Steam ID: nothing is sent to or accepted from it. */
		bool bRefused = false;
		FChannelRule Transmit;
		FChannelRule Listen;

		bool operator==(const FPlayerRules& Other) const
		{
			return bCanTalk == Other.bCanTalk && bOwnerVoice == Other.bOwnerVoice && bRefused == Other.bRefused && Transmit == Other.Transmit && Listen == Other.Listen;
		}
	};

	/** Where one voice packet goes: the talker's pawn (mouth) and channel ids (sorted, unique). */
	struct FVoiceRoute
	{
		bool bMouth = false;
		TArray<int32, TInlineAllocator<MaxPacketChannels>> Channels;

		bool IsEmpty() const { return !bMouth && Channels.IsEmpty(); }

		bool operator==(const FVoiceRoute& Other) const { return bMouth == Other.bMouth && Channels == Other.Channels; }
		bool operator!=(const FVoiceRoute& Other) const { return !(*this == Other); }
	};

	/**
	 * What a talker may send at all (shared by sender, receiver and relay): nothing when server muted or refused, no mouth without
	 * owner voice, only the channels Transmit allows. Out is overwritten.
	 */
	void FilterForTalker(const FPlayerRules& Talker, bool bMouth, TConstArrayView<int32> Channels, FVoiceRoute& Out);

	/** What one listener may get of Route: nothing when refused, the mouth only when bMouthInRange, only the channels Listen allows. Out is overwritten. */
	void FilterForListener(const FPlayerRules& Listener, bool bMouthInRange, const FVoiceRoute& Route, FVoiceRoute& Out);

	/** Sender hysteresis: a listener out of range comes back in at the distance, and an in-range one goes out beyond it plus this share. */
	constexpr float OwnerVoiceHysteresis = 0.1f;

	/** Receiver margin: a mouth is dropped only beyond the distance plus this share, so packets an honest sender sent at its edge still play. */
	constexpr float OwnerVoiceReceiveSlack = 0.2f;

	/** How far a talker's mouth carries: the talker's override when above 0, no limit when below 0, else Global. 0 = no limit. */
	float ResolveOwnerVoiceDistance(float Global, float TalkerOverride);

	/**
	 * Sender: whether a listener is in range of our mouth now. bKnown: both pawn locations are known (else out of range).
	 * MaxDistance 0 = no limit (always in range).
	 */
	bool UpdateMouthInRange(bool bWasInRange, bool bKnown, double DistanceSquared, float MaxDistance);

	/** Receiver: whether a talker's mouth may play here. Unknown locations never do while a limit is set. MaxDistance 0 = no limit. */
	bool IsMouthAcceptable(bool bKnown, double DistanceSquared, float MaxDistance);

	/** Order independent hash of a channel tag map (tag names and ids), compared between server and clients. 0 for an empty map. */
	uint32 HashChannelMap(const TMap<FGameplayTag, int32>& TagMap);

	/** The ids that appear more than once in Ids (two players with one Steam ID). Out is overwritten. */
	void FindDuplicateIds(TConstArrayView<int64> Ids, TSet<int64>& Out);
}
