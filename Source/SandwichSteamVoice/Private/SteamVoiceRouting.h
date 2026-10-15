// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/MultithreadedPatching.h"

/**
 * What the game thread and the voice worker exchange. The game thread publishes immutable routing snapshots (who is in the game,
 * who is muted, whether to record); the worker posts events back (talker audio started / idle, talking, flood).
 * Neither side ever touches the other's state directly.
 */
namespace SandwichSteam::Voice
{
	/**
	 * Fake talkers use Steam IDs FakeTalkerIdBase + index: well formed individual accounts (public universe, individual type,
	 * desktop instance) at the very top of the account range, because Steam's decoder may refuse a malformed speaker id.
	 */
	constexpr uint64 FakeTalkerIdBase = 0x01100001FFFFFF00ull;
	constexpr int32 MaxFakeTalkers = 8;

	FORCEINLINE bool IsFakeTalkerId(uint64 Id)
	{
		return Id >= FakeTalkerIdBase && Id < FakeTalkerIdBase + MaxFakeTalkers;
	}

	/** Steam.Voice.FakeTalker: the local capture re-injected as fake talkers at the receive step (non-Shipping). */
	struct FFakeTalkerConfig
	{
		int32 Count = 0;
		/** Fake talker i plays DelayMs + i * StaggerMs late. A wide stagger makes the copies easy to tell apart by ear; they interleave in the decoder either way. */
		int32 DelayMs = 100;
		int32 StaggerMs = 250;
		float LossPercent = 0.0f;
		/** -1: the fakes send what we send (mouth and transmit channels). A channel id: they talk on that channel only (radio), for testing Channel speakers. */
		int32 Channel = -1;

		bool operator==(const FFakeTalkerConfig& Other) const
		{
			return Count == Other.Count && DelayMs == Other.DelayMs && StaggerMs == Other.StaggerMs && LossPercent == Other.LossPercent && Channel == Other.Channel;
		}
	};

	using FPatchSplitterPtr = TSharedPtr<Audio::FPatchSplitter, ESPMode::ThreadSafe>;

	/**
	 * One mix the game thread wants from the worker: everyone on Channel, minus Excluded (0 = nobody, the full mix).
	 * The game thread creates the splitter and connects the Channel speakers to it; the worker pushes the mix into it.
	 */
	struct FChannelOutput
	{
		int32 Channel = INDEX_NONE;
		uint64 Excluded = 0;
		FPatchSplitterPtr Splitter;

		bool operator==(const FChannelOutput& Other) const
		{
			return Channel == Other.Channel && Excluded == Other.Excluded && Splitter == Other.Splitter;
		}
	};

	/** Immutable once published. Steam IDs as raw uint64. */
	struct FRoutingSnapshot
	{
		uint64 LocalId = 0;
		/** Microphone open (push to talk held or open mic). */
		bool bRecord = false;
		bool bMuteSteamFriendsWhileTalking = false;
		/** Our voice plays on our own pawn (packet Mouth flag). */
		bool bTransmitMouth = true;
		/** The channels we transmit on: resolved ids, unique, sorted, at most MaxPacketChannels. Travel in every packet. */
		TArray<int32> TransmitChannels;
		/** The other players of the current game: the only peers we send to and accept. */
		TArray<uint64> Peers;
		/** Mute: their audio is dropped here, we still send to them. */
		TSet<uint64> Muted;
		/** Blocked on Steam: dropped both ways. */
		TSet<uint64> Blocked;
		FFakeTalkerConfig Fake;
		/** Steam.Voice.SimLoss: percent of real received packets dropped (non-Shipping). */
		float SimLossPercent = 0.0f;
		/** The channel mixes Channel speakers play, sorted by channel. Channels nobody plays are not decoded or mixed. */
		TArray<FChannelOutput> ChannelOutputs;
	};

	using FRoutingSnapshotPtr = TSharedPtr<const FRoutingSnapshot, ESPMode::ThreadSafe>;

	/** Fixed for the life of one worker (from the settings). */
	struct FWorkerConfig
	{
		int32 MessageChannel = 7;
		/** 0 = Steam's optimal rate. */
		int32 RequestedSampleRate = 0;
		float PacketsPerSecond = 60.0f;
		float BytesPerSecond = 8192.0f;
		/** Voice activation level the worker starts with (RMS, 0 = off). Changed at runtime with FSteamVoiceWorker::SetActivationThreshold. */
		float ActivationThreshold = 0.0f;
		/** How long the gate stays open after the level fell below the threshold. */
		float ActivationHoldSeconds = 0.3f;
		/** How far ahead of the mix clock a talker starts on a channel (absorbs network jitter, like a mouth speaker's playout delay). */
		float ChannelLeadSeconds = 0.08f;
	};

	enum class EWorkerEvent : uint8
	{
		/** The worker knows a new talker (first packet, usually the session hello): connect a speaker to Splitter now, so the first word is not lost. */
		TalkerAdded,
		/** A talker's decoded audio started: start the speaker. */
		TalkerActive,
		/** No audio for a second: stop the speaker. */
		TalkerIdle,
		/** Audio within the last 0.3 s (talking indicator). Also sent for the local player (capture). */
		TalkingStarted,
		TalkingStopped,
		/** Over the rate limit for 5 s in a row: muted for the session. */
		Flood,
		/** The talker left (removed from the worker). */
		TalkerRemoved,
		/** A channel output (Channel, Talker = excluded player, Splitter = the output's) got audio: start its speakers. */
		ChannelActive,
		/** A channel output had no audio for a second: stop its speakers. */
		ChannelIdle
	};

	struct FWorkerEvent
	{
		EWorkerEvent Type = EWorkerEvent::TalkerIdle;
		/** The talker; for channel events the output's excluded player (0 = full mix). */
		uint64 Talker = 0;
		/** Channel events only. */
		int32 Channel = INDEX_NONE;
		bool bFake = false;
		int32 SampleRate = 0;
		FPatchSplitterPtr Splitter;
	};

	/** Per talker numbers for Steam.Voice.Dump. */
	struct FTalkerStats
	{
		uint64 Talker = 0;
		bool bFake = false;
		bool bActive = false;
		bool bTalking = false;
		uint64 Packets = 0;
		uint64 Lost = 0;
		uint64 Late = 0;
		uint64 Duplicate = 0;
		uint64 Resyncs = 0;
		uint64 FloodDrops = 0;
		uint64 DecodeErrors = 0;
		/** Last DecompressVoice result (EVoiceResult, 0 = OK) and decoded samples of the last packet. */
		int32 LastDecodeResult = 0;
		int32 LastDecodedSamples = 0;
		bool bFlooded = false;
		int32 Queued = 0;
		float Level = 0.0f;
		/** Capture to speaker buffer (fake talkers only, ms). -1 = unknown. */
		float PipelineLatencyMs = -1.0f;
	};

	/** Per channel output numbers for Steam.Voice.Dump. */
	struct FChannelOutputStats
	{
		int32 Channel = INDEX_NONE;
		uint64 Excluded = 0;
		bool bRunning = false;
		bool bActive = false;
		int32 ActiveTalkers = 0;
		float Gain = 1.0f;
		uint64 Limited = 0;
		uint64 OverflowSamples = 0;
	};

	/** Worker numbers for Steam.Voice.Dump, copied out under a lock a few times per second. */
	struct FWorkerStats
	{
		bool bRunning = false;
		bool bRecording = false;
		int32 OptimalSampleRate = 0;
		int32 DecodeSampleRate = 0;
		int32 LastCaptureResult = 0;
		uint64 CapturedChunks = 0;
		uint64 CapturedBytes = 0;
		uint64 CaptureTooLarge = 0;
		/** Captured packets not sent because they were below the voice activation threshold. */
		uint64 GatedChunks = 0;
		/** Captured packets whose level could not be measured (sent anyway). */
		uint64 LevelDecodeErrors = 0;
		float ActivationThreshold = 0.0f;
		/** Format check of the captured Steam payloads: starts with our Steam ID / ends with a valid CRC-32. */
		uint64 PayloadIdMatches = 0;
		uint64 PayloadCrcMatches = 0;
		uint64 PacketsSent = 0;
		uint64 SendFailures = 0;
		/** Captured packets not sent because we transmit to neither the mouth nor a channel. */
		uint64 NotTransmitted = 0;
		uint64 HellosSent = 0;
		uint64 PacketsReceived = 0;
		uint64 Invalid = 0;
		uint64 FromUnknown = 0;
		/** Received payloads whose embedded speaker is not the sender. Dropped. */
		uint64 SpeakerMismatch = 0;
		/** Fake talker packets not rewritten because the payload did not have the expected id + CRC (they keep ours: decoder check not valid). */
		uint64 FakeRewriteFailed = 0;
		/** Fake talker packets with their own speaker id. */
		uint64 FakeRewritten = 0;
		uint64 MutedDrops = 0;
		uint64 SimLossDrops = 0;
		/**
		 * Decoder check (two or more fake talkers): every fake decodes the same packets, interleaved with the others. With separate decoder
		 * state per talker the decoded samples of one packet are identical for every fake; shared state makes them differ.
		 */
		uint64 DecodeCompared = 0;
		uint64 DecodeIdentical = 0;
		uint64 DecodeLengthMismatch = 0;
		int32 DecodeMaxDifference = 0;
		uint64 SessionsAccepted = 0;
		uint64 SessionsRejected = 0;
		int32 SessionsPending = 0;
		float LoopMaxMs = 0.0f;
		float LoopAverageMs = 0.0f;
		/** Received voice packets with neither mouth nor a channel anybody here plays: not decoded. */
		uint64 NotPlayed = 0;
		TArray<FTalkerStats> Talkers;
		TArray<FChannelOutputStats> Channels;
	};
}
