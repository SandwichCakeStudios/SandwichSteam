// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"
#include "HAL/Runnable.h"
#include "Math/RandomStream.h"
#include "SteamVoiceActivationGate.h"
#include "SteamVoiceChannelMixer.h"
#include "SteamVoiceJitterBuffer.h"
#include "SteamVoicePacket.h"
#include "SteamVoiceRateLimiter.h"
#include "SteamVoiceRouting.h"

#include <atomic>

class FEvent;
class FRunnableThread;
class FSteamVoiceTransport;

/**
 * The real-time half of Steam voice, on its own thread (10 ms loop), so game-thread hitches (loading, GC, streaming) never stop
 * or burst voice. Never touches a UObject.
 *
 * Each loop: routing snapshot -> session requests -> hello -> capture (GetVoice), level, voice activation and send -> receive -> validate, rate limit,
 * mute -> jitter (reorder) -> DecompressVoice -> talker patch splitter (mouth) and channel mixes -> channel output splitters -> talking / activity events.
 *
 * Game thread API: Start, Shutdown, Publish, PopEvent, GetStats, SetActivationThreshold, GetLocalLevel.
 */
class FSteamVoiceWorker : public FRunnable
{
public:
	FSteamVoiceWorker(const SandwichSteam::Voice::FWorkerConfig& InConfig, TSharedRef<FSteamVoiceTransport> InTransport);
	virtual ~FSteamVoiceWorker() override;

	FSteamVoiceWorker(const FSteamVoiceWorker&) = delete;
	FSteamVoiceWorker& operator=(const FSteamVoiceWorker&) = delete;

	/** Starts the thread. False when it could not be created. */
	bool StartThread();

	/** Stops the thread and waits for it (closes the microphone and the sessions on the way out). Safe to call twice. */
	void Shutdown();

	/** Hands a new routing snapshot to the worker (game thread). */
	void Publish(SandwichSteam::Voice::FRoutingSnapshotPtr Snapshot);

	/** Next event from the worker (game thread). */
	bool PopEvent(SandwichSteam::Voice::FWorkerEvent& OutEvent) { return Events.Dequeue(OutEvent); }

	/** Copy of the latest numbers (any thread). */
	SandwichSteam::Voice::FWorkerStats GetStats() const;

	/** Voice activation level (RMS, 0 to 1; 0 = off). Captured packets below it are not sent. Any thread, applies to the next packet. */
	void SetActivationThreshold(float Threshold) { ActivationThreshold.store(FMath::Clamp(Threshold, 0.0f, 1.0f), std::memory_order_relaxed); }

	/** Level (RMS, 0 to 1) of the last captured packet, 0 while nothing is captured. Any thread. */
	float GetLocalLevel() const { return LocalLevel.load(std::memory_order_relaxed); }

	/** The rate the worker decodes at; fixed for its life. */
	int32 GetDecodeSampleRate() const { return DecodeSampleRate; }

	//~ Begin FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;
	//~ End FRunnable

private:
	struct FTalker
	{
		uint64 Id = 0;
		bool bFake = false;
		bool bActive = false;
		bool bTalking = false;
		bool bFlooded = false;
		/** Any decoded audio (talking indicator). */
		double LastAudio = -1.0;
		/** Decoded audio for the mouth (the talker's speakers start and stop on it). */
		double LastMouthAudio = -1.0;
		float Level = 0.0f;
		float LatencyMs = -1.0f;
		uint64 FloodDrops = 0;
		uint64 DecodeErrors = 0;
		int32 LastDecodeResult = 0;
		int32 LastDecodedSamples = 0;
		SandwichSteam::Voice::FRateLimiter Limiter;
		SandwichSteam::Voice::FJitterBuffer Jitter;
		SandwichSteam::Voice::FPatchSplitterPtr Splitter;
	};

	/** One channel the game thread wants mixed, with its outputs (full mix, mixes minus a player) in the mixer's output order. */
	struct FChannel
	{
		SandwichSteam::Voice::FChannelMixer Mixer;
		TArray<SandwichSteam::Voice::FPatchSplitterPtr> Splitters;
		TArray<bool> Active;
	};

	struct FDelayedPacket
	{
		double Due = 0.0;
		double Stamp = 0.0;
		uint64 Talker = 0;
		TArray<uint8> Data;
	};

	void Step(double Now);
	void ApplySnapshot(double Now);
	void ProcessSessions(double Now);
	void SendHellos(double Now);
	void Capture(double Now);
	void HandleCaptured(const uint8* Data, int32 Size, double Now);
	void SendCaptured(const uint8* Data, int32 Size, double Now, bool bIdMatches, bool bCrcMatches);
	/** Decodes a captured packet of ours and returns its level (RMS), or -1 when it could not be decoded. */
	float MeasureCaptured(const uint8* Data, int32 Size);
	void HandleIncoming(uint64 Sender, const uint8* Data, int32 Size, double Now, bool bFake, double Stamp);
	void DeliverFakePackets(double Now);
	void Playout(double Now);
	/** Decodes one played packet and routes it to the mouth and to the channels it carries. */
	void Decode(FTalker& Talker, const SandwichSteam::Voice::FPacketView& Packet, double Now, double Stamp);
	void ApplyChannelOutputs();
	void MixChannels(double Now);
	void CheckDecode(double Stamp, const int16* Samples, int32 NumSamples, double Now);
	void UpdateActivity(double Now);
	void PublishStats(double Now);
	void CloseAll();

	FTalker& FindOrAddTalker(uint64 Id, bool bFake);
	void RemoveTalker(uint64 Id);
	void PostEvent(SandwichSteam::Voice::EWorkerEvent Type, const FTalker* Talker, uint64 Id);
	void PostChannelEvent(SandwichSteam::Voice::EWorkerEvent Type, int32 Channel, uint64 Excluded, const SandwichSteam::Voice::FPatchSplitterPtr& Splitter);

	bool IsPeer(uint64 Id) const { return PeerSet.Contains(Id); }

	const SandwichSteam::Voice::FWorkerConfig Config;
	TSharedRef<FSteamVoiceTransport> Transport;
	int32 OptimalSampleRate = 0;
	int32 DecodeSampleRate = 48000;

	TUniquePtr<FRunnableThread> Thread;
	FEvent* WakeEvent = nullptr;
	std::atomic<bool> bStopRequested{false};

	// Game thread -> worker.
	mutable FCriticalSection SnapshotLock;
	SandwichSteam::Voice::FRoutingSnapshotPtr PendingSnapshot;
	std::atomic<bool> bSnapshotDirty{false};
	std::atomic<float> ActivationThreshold{0.0f};

	// Worker -> any thread.
	std::atomic<float> LocalLevel{0.0f};

	// Worker -> game thread.
	TQueue<SandwichSteam::Voice::FWorkerEvent, EQueueMode::Spsc> Events;
	mutable FCriticalSection StatsLock;
	SandwichSteam::Voice::FWorkerStats PublishedStats;

	// Worker thread only.
	SandwichSteam::Voice::FRoutingSnapshotPtr Snapshot;
	TSet<uint64> PeerSet;
	TMap<uint64, TUniquePtr<FTalker>> Talkers;
	TMap<int32, TUniquePtr<FChannel>> Channels;
	TMap<uint64, double> PendingSessions;
	TArray<FDelayedPacket> DelayedPackets;

	/** Decoder check: the first fake's decoded samples of each captured packet (keyed by capture stamp), compared with the other fakes. */
	struct FDecodeReference
	{
		TArray<int16> Samples;
		double Created = 0.0;
		int32 Seen = 1;
	};
	TMap<uint64, FDecodeReference> DecodeReferences;
	FRandomStream Random;
	SandwichSteam::Voice::FWorkerStats Stats;
	SandwichSteam::Voice::FActivationGate Gate;
	/** The last packet the gate held back, with the format check results it was captured with. Sent first when the gate opens. */
	TArray<uint8> PreRollBuffer;
	bool bPreRollIdMatches = false;
	bool bPreRollCrcMatches = false;
	TArray<uint8> CaptureBuffer;
	TArray<uint8> PacketBuffer;
	TArray<uint8> HelloBuffer;
	TArray<int16> PcmBuffer;
	TArray<float> FloatBuffer;
	uint16 Sequence = 0;
	bool bRecording = false;
	bool bDrainingTail = false;
	bool bSpeakingFlagSet = false;
	bool bLocalTalking = false;
	double LastLocalAudio = -1.0;
	double LastCapture = -1.0;
	double NextHello = 0.0;
	double NextStats = 0.0;
	double LoopTotalMs = 0.0;
	uint64 LoopCount = 0;
};
