// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoiceWorker.h"
#include "Core/SteamLog.h"
#include "Core/SteamSDK.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/RunnableThread.h"
#include "SteamVoicePacket.h"
#include "SteamVoiceTransport.h"

using namespace SandwichSteam::Voice;

namespace
{
	constexpr double LoopSeconds = 0.010;
	/** How long a missing packet is waited for once later ones arrived. */
	constexpr double MaxGapWaitSeconds = 0.060;
	/** Audio within this time counts as talking. */
	constexpr double TalkingHoldSeconds = 0.3;
	/** No audio for this long: the talker's speaker stops. */
	constexpr double IdleSeconds = 1.0;
	constexpr double HelloIntervalSeconds = 10.0;
	/** A session request from somebody who is not (yet) a player waits this long for them to appear (late joiner race). */
	constexpr double PendingSessionSeconds = 5.0;
	constexpr double StatsIntervalSeconds = 0.25;
	/** Decoder check references older than this are dropped (their packet was lost for some fake). */
	constexpr double DecodeReferenceSeconds = 2.0;
	/** Steam's compressed voice for one loop is a few hundred bytes; 8 KB covers a long stall. */
	constexpr int32 CaptureBufferBytes = 8 * 1024;
	/** Decoded samples for one packet (one second at 48 kHz). */
	constexpr int32 MaxDecodedSamples = 48000;
	constexpr int32 MaxCaptureReadsPerLoop = 8;

	float ComputeRms(const int16* Samples, int32 NumSamples)
	{
		if (NumSamples <= 0)
		{
			return 0.0f;
		}

		float SumSquares = 0.0f;
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const float Sample = static_cast<float>(Samples[Index]) / 32768.0f;
			SumSquares += Sample * Sample;
		}
		return FMath::Sqrt(SumSquares / static_cast<float>(NumSamples));
	}
}

FSteamVoiceWorker::FSteamVoiceWorker(const FWorkerConfig& InConfig, TSharedRef<FSteamVoiceTransport> InTransport)
	: Config(InConfig)
	, Transport(MoveTemp(InTransport))
	, Random(static_cast<int32>(FPlatformTime::Cycles()))
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (ISteamUser* User = SteamUser())
	{
		OptimalSampleRate = static_cast<int32>(User->GetVoiceOptimalSampleRate());
	}
#endif

	const int32 Wanted = Config.RequestedSampleRate > 0 ? Config.RequestedSampleRate : OptimalSampleRate;
	DecodeSampleRate = Wanted > 0 ? FMath::Clamp(Wanted, 8000, 48000) : 48000;

	CaptureBuffer.SetNumUninitialized(CaptureBufferBytes);
	PacketBuffer.Reserve(MaxPacketBytes);
	PreRollBuffer.Reserve(MaxPacketBytes);
	PcmBuffer.SetNumUninitialized(MaxDecodedSamples);
	FloatBuffer.SetNumUninitialized(MaxDecodedSamples);
	WriteHelloPacket(HelloBuffer);

	Gate.Configure(Config.ActivationHoldSeconds);
	SetActivationThreshold(Config.ActivationThreshold);

	Stats.OptimalSampleRate = OptimalSampleRate;
	Stats.DecodeSampleRate = DecodeSampleRate;
	PublishedStats = Stats;
}

FSteamVoiceWorker::~FSteamVoiceWorker()
{
	Shutdown();
}

bool FSteamVoiceWorker::StartThread()
{
	check(IsInGameThread());
	if (Thread)
	{
		return true;
	}

	WakeEvent = FPlatformProcess::GetSynchEventFromPool(false);
	bStopRequested.store(false);
	Thread.Reset(FRunnableThread::Create(this, TEXT("SandwichSteamVoice"), 0, TPri_AboveNormal));
	if (!Thread)
	{
		FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
		WakeEvent = nullptr;
		return false;
	}
	return true;
}

void FSteamVoiceWorker::Shutdown()
{
	if (Thread)
	{
		Stop();
		Thread->WaitForCompletion();
		Thread.Reset();
	}
	if (WakeEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WakeEvent);
		WakeEvent = nullptr;
	}
}

void FSteamVoiceWorker::Stop()
{
	bStopRequested.store(true);
	if (WakeEvent)
	{
		WakeEvent->Trigger();
	}
}

void FSteamVoiceWorker::Publish(FRoutingSnapshotPtr NewSnapshot)
{
	FScopeLock Lock(&SnapshotLock);
	PendingSnapshot = MoveTemp(NewSnapshot);
	bSnapshotDirty.store(true, std::memory_order_release);
}

FWorkerStats FSteamVoiceWorker::GetStats() const
{
	FScopeLock Lock(&StatsLock);
	return PublishedStats;
}

uint32 FSteamVoiceWorker::Run()
{
	Stats.bRunning = true;
	while (!bStopRequested.load())
	{
		const double Start = FPlatformTime::Seconds();
		Step(Start);

		const double ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		LoopTotalMs += ElapsedMs;
		++LoopCount;
		Stats.LoopMaxMs = FMath::Max(Stats.LoopMaxMs, static_cast<float>(ElapsedMs));

		const int32 WaitMs = FMath::Max(static_cast<int32>(LoopSeconds * 1000.0 - ElapsedMs), 1);
		WakeEvent->Wait(WaitMs);
	}

	CloseAll();
	Stats.bRunning = false;
	PublishStats(FPlatformTime::Seconds());
	return 0;
}

void FSteamVoiceWorker::Step(double Now)
{
	ApplySnapshot(Now);
	if (!Snapshot.IsValid())
	{
		return;
	}

	ProcessSessions(Now);
	SendHellos(Now);
	Capture(Now);
	Transport->Receive([this, Now](uint64 Sender, const uint8* Data, int32 Size)
	{
		HandleIncoming(Sender, Data, Size, Now, false, 0.0);
	});
	DeliverFakePackets(Now);
	Playout(Now);
	UpdateActivity(Now);

	if (Now >= NextStats)
	{
		NextStats = Now + StatsIntervalSeconds;
		PublishStats(Now);
	}
}

// ---- Routing ----

void FSteamVoiceWorker::ApplySnapshot(double Now)
{
	if (!bSnapshotDirty.load(std::memory_order_acquire))
	{
		return;
	}

	FRoutingSnapshotPtr NewSnapshot;
	{
		FScopeLock Lock(&SnapshotLock);
		NewSnapshot = PendingSnapshot;
		bSnapshotDirty.store(false, std::memory_order_relaxed);
	}
	if (!NewSnapshot.IsValid())
	{
		return;
	}

	const bool bPeersChanged = !Snapshot.IsValid() || Snapshot->Peers != NewSnapshot->Peers;
	const bool bFakeChanged = !Snapshot.IsValid() || !(Snapshot->Fake == NewSnapshot->Fake);
	Snapshot = MoveTemp(NewSnapshot);

	if (bFakeChanged)
	{
		// A new fake talker test starts a new decoder check.
		DecodeReferences.Reset();
		Stats.DecodeCompared = 0;
		Stats.DecodeIdentical = 0;
		Stats.DecodeLengthMismatch = 0;
		Stats.DecodeMaxDifference = 0;
	}

	if (bPeersChanged)
	{
		PeerSet.Reset();
		PeerSet.Append(Snapshot->Peers);
		NextHello = Now; // Say hello to newcomers right away.
	}

	// Talkers who left the game, and fake talkers beyond the configured count, go away.
	TArray<uint64, TInlineAllocator<16>> Gone;
	for (const TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		const FTalker& Talker = *Pair.Value;
		const bool bKeep = Talker.bFake
			? (Talker.Id - FakeTalkerIdBase) < static_cast<uint64>(Snapshot->Fake.Count)
			: PeerSet.Contains(Talker.Id);
		if (!bKeep)
		{
			Gone.Add(Talker.Id);
		}
	}
	for (const uint64 Id : Gone)
	{
		RemoveTalker(Id);
	}

	if (Snapshot->Fake.Count <= 0)
	{
		DelayedPackets.Reset();
	}
}

void FSteamVoiceWorker::ProcessSessions(double Now)
{
	uint64 Peer = 0;
	while (Transport->PopSessionRequest(Peer))
	{
		if (IsPeer(Peer) && !Snapshot->Blocked.Contains(Peer))
		{
			Transport->AcceptSession(Peer);
			++Stats.SessionsAccepted;
			PendingSessions.Remove(Peer);
		}
		else if (!PendingSessions.Contains(Peer))
		{
			PendingSessions.Add(Peer, Now);
		}
	}

	for (auto It = PendingSessions.CreateIterator(); It; ++It)
	{
		if (IsPeer(It.Key()) && !Snapshot->Blocked.Contains(It.Key()))
		{
			Transport->AcceptSession(It.Key());
			++Stats.SessionsAccepted;
			It.RemoveCurrent();
		}
		else if (Now - It.Value() > PendingSessionSeconds)
		{
			Transport->CloseSession(It.Key());
			++Stats.SessionsRejected;
			It.RemoveCurrent();
		}
	}
	Stats.SessionsPending = PendingSessions.Num();
}

void FSteamVoiceWorker::SendHellos(double Now)
{
	if (Now < NextHello)
	{
		return;
	}
	NextHello = Now + HelloIntervalSeconds;

	for (const uint64 Peer : Snapshot->Peers)
	{
		if (!Snapshot->Blocked.Contains(Peer) && Transport->SendHello(Peer, HelloBuffer.GetData(), HelloBuffer.Num()))
		{
			++Stats.HellosSent;
		}
	}
}

// ---- Capture and send ----

void FSteamVoiceWorker::Capture(double Now)
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	ISteamUser* User = SteamUser();
	if (!User)
	{
		return;
	}

	const bool bWant = Snapshot->bRecord;
	if (bWant && !bRecording)
	{
		User->StartVoiceRecording();
		bRecording = true;
		bDrainingTail = false;
		Gate.Reset();
		if (Snapshot->bMuteSteamFriendsWhileTalking && Snapshot->LocalId != 0)
		{
			if (ISteamFriends* Friends = SteamFriends())
			{
				Friends->SetInGameVoiceSpeaking(CSteamID(Snapshot->LocalId), true);
				bSpeakingFlagSet = true;
			}
		}
	}
	else if (!bWant && bRecording)
	{
		// Steam records a short tail after the stop: keep reading until it says NotRecording.
		User->StopVoiceRecording();
		bRecording = false;
		bDrainingTail = true;
		if (bSpeakingFlagSet)
		{
			if (ISteamFriends* Friends = SteamFriends())
			{
				Friends->SetInGameVoiceSpeaking(CSteamID(Snapshot->LocalId), false);
			}
			bSpeakingFlagSet = false;
		}
	}
	Stats.bRecording = bRecording;

	if (!bRecording && !bDrainingTail)
	{
		return;
	}

	for (int32 Read = 0; Read < MaxCaptureReadsPerLoop; ++Read)
	{
		uint32 BytesWritten = 0;
		const EVoiceResult Result = User->GetVoice(true, CaptureBuffer.GetData(), static_cast<uint32>(CaptureBuffer.Num()), &BytesWritten);
		Stats.LastCaptureResult = static_cast<int32>(Result);

		if (Result == k_EVoiceResultNotRecording)
		{
			bDrainingTail = false;
			break;
		}
		if (Result != k_EVoiceResultOK || BytesWritten == 0)
		{
			break;
		}
		HandleCaptured(CaptureBuffer.GetData(), static_cast<int32>(BytesWritten), Now);
	}
#endif
}

void FSteamVoiceWorker::HandleCaptured(const uint8* Data, int32 Size, double Now)
{
	++Stats.CapturedChunks;
	Stats.CapturedBytes += static_cast<uint64>(Size);
	LastCapture = Now;

	const TConstArrayView<uint8> Captured(Data, Size);
	const bool bIdMatches = Snapshot->LocalId != 0 && ReadPayloadSpeaker(Captured) == Snapshot->LocalId;
	const bool bCrcMatches = HasValidPayloadCrc(Captured);
	Stats.PayloadIdMatches += bIdMatches ? 1 : 0;
	Stats.PayloadCrcMatches += bCrcMatches ? 1 : 0;

	// Steam has no input gain: our own packet is decoded once to measure it (level meter, voice activation).
	const float Level = MeasureCaptured(Data, Size);
	LocalLevel.store(FMath::Max(Level, 0.0f), std::memory_order_relaxed);

	Gate.SetThreshold(ActivationThreshold.load(std::memory_order_relaxed));
	switch (Gate.Evaluate(Level, Now))
	{
	case FActivationGate::EDecision::Hold:
		PreRollBuffer.Reset();
		PreRollBuffer.Append(Data, Size);
		bPreRollIdMatches = bIdMatches;
		bPreRollCrcMatches = bCrcMatches;
		return;

	case FActivationGate::EDecision::SendWithPreRoll:
		SendCaptured(PreRollBuffer.GetData(), PreRollBuffer.Num(), Now, bPreRollIdMatches, bPreRollCrcMatches);
		break;

	default:
		break;
	}
	SendCaptured(Data, Size, Now, bIdMatches, bCrcMatches);
}

float FSteamVoiceWorker::MeasureCaptured(const uint8* Data, int32 Size)
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	ISteamUser* User = SteamUser();
	if (!User)
	{
		return -1.0f;
	}

	// Our own speaker id has its own decoder state: nobody else's audio passes through it (received packets with our id are dropped).
	uint32 BytesWritten = 0;
	const EVoiceResult Result = User->DecompressVoice(Data, static_cast<uint32>(Size), PcmBuffer.GetData(),
		static_cast<uint32>(PcmBuffer.Num() * sizeof(int16)), &BytesWritten, static_cast<uint32>(DecodeSampleRate));
	if (Result != k_EVoiceResultOK)
	{
		++Stats.LevelDecodeErrors;
		return -1.0f;
	}
	return ComputeRms(PcmBuffer.GetData(), FMath::Min(static_cast<int32>(BytesWritten / sizeof(int16)), MaxDecodedSamples));
#else
	return -1.0f;
#endif
}

void FSteamVoiceWorker::SendCaptured(const uint8* Data, int32 Size, double Now, bool bIdMatches, bool bCrcMatches)
{
	LastLocalAudio = Now;

	const uint16 PacketSequence = Sequence++;
	if (!WriteVoicePacket(PacketSequence, PacketFlags::Mouth, TConstArrayView<int32>(), TConstArrayView<uint8>(Data, Size), PacketBuffer))
	{
		++Stats.CaptureTooLarge;
		return;
	}

	for (const uint64 Peer : Snapshot->Peers)
	{
		if (Snapshot->Blocked.Contains(Peer))
		{
			continue;
		}
		if (Transport->SendVoice(Peer, PacketBuffer.GetData(), PacketBuffer.Num()))
		{
			++Stats.PacketsSent;
		}
		else
		{
			++Stats.SendFailures;
		}
	}

	// Fake talkers: the same packet comes back later as somebody else, through the full receive path.
	const FFakeTalkerConfig& Fake = Snapshot->Fake;
	for (int32 Index = 0; Index < FMath::Min(Fake.Count, MaxFakeTalkers); ++Index)
	{
		if (Fake.LossPercent > 0.0f && Random.FRand() * 100.0f < Fake.LossPercent)
		{
			continue;
		}

		FDelayedPacket& Delayed = DelayedPackets.AddDefaulted_GetRef();
		Delayed.Due = Now + (Fake.DelayMs + Index * Fake.StaggerMs) / 1000.0;
		Delayed.Stamp = Now;
		Delayed.Talker = FakeTalkerIdBase + static_cast<uint64>(Index);
		Delayed.Data = PacketBuffer;

		// Steam decodes per speaker id inside the payload: give each fake its own id, or all fakes would share our decoder.
		// Only when this payload has exactly the expected layout; otherwise the rewrite would damage it.
		const TArrayView<uint8> FakePayload(Delayed.Data.GetData() + PacketHeaderBytes, Delayed.Data.Num() - PacketHeaderBytes);
		if (bIdMatches && bCrcMatches && RewritePayloadSpeaker(FakePayload, Delayed.Talker))
		{
			++Stats.FakeRewritten;
		}
		else
		{
			++Stats.FakeRewriteFailed;
		}
	}
}

// ---- Receive ----

void FSteamVoiceWorker::HandleIncoming(uint64 Sender, const uint8* Data, int32 Size, double Now, bool bFake, double Stamp)
{
	if (!bFake)
	{
		++Stats.PacketsReceived;
		if (!IsPeer(Sender))
		{
			++Stats.FromUnknown;
			return;
		}
		if (Snapshot->SimLossPercent > 0.0f && Random.FRand() * 100.0f < Snapshot->SimLossPercent)
		{
			++Stats.SimLossDrops;
			return;
		}
	}

	FTalker& Talker = FindOrAddTalker(Sender, bFake);
	if (Talker.bFlooded)
	{
		++Talker.FloodDrops;
		return;
	}
	if (!Talker.Limiter.TryConsume(Size, Now))
	{
		++Talker.FloodDrops;
		if (Talker.Limiter.IsFlooding())
		{
			Talker.bFlooded = true;
			PostEvent(EWorkerEvent::Flood, &Talker, Talker.Id);
		}
		return;
	}

	FPacketView Packet;
	const EPacketReadResult Result = ReadPacket(Data, Size, Packet);
	if (Result == EPacketReadResult::Hello)
	{
		return;
	}
	if (Result != EPacketReadResult::Voice)
	{
		++Stats.Invalid;
		return;
	}

	if (Snapshot->Muted.Contains(Sender) || Snapshot->Blocked.Contains(Sender))
	{
		++Stats.MutedDrops;
		return;
	}

	// Only the mouth plays for now; channel playback is not built yet.
	if (!Packet.HasMouth())
	{
		return;
	}

	const TConstArrayView<uint8> Payload(Packet.Payload, Packet.PayloadSize);
	if (ReadPayloadSpeaker(Payload) != Sender)
	{
		// Steam decodes per speaker id inside the payload: a packet carrying somebody else's id would drive that player's decoder state.
		++Stats.SpeakerMismatch;
		return;
	}

	Talker.Jitter.Push(Packet.Sequence, Payload, Now, Stamp);
}

void FSteamVoiceWorker::DeliverFakePackets(double Now)
{
	for (int32 Index = 0; Index < DelayedPackets.Num();)
	{
		FDelayedPacket& Delayed = DelayedPackets[Index];
		if (Delayed.Due <= Now)
		{
			HandleIncoming(Delayed.Talker, Delayed.Data.GetData(), Delayed.Data.Num(), Now, true, Delayed.Stamp);
			DelayedPackets.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
		else
		{
			++Index;
		}
	}
}

// ---- Playout ----

void FSteamVoiceWorker::Playout(double Now)
{
	for (TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		FTalker& Talker = *Pair.Value;
		for (;;)
		{
			TConstArrayView<uint8> Payload;
			double Stamp = 0.0;
			const FJitterBuffer::EPopResult Result = Talker.Jitter.Pop(Now, MaxGapWaitSeconds, Payload, Stamp);
			if (Result == FJitterBuffer::EPopResult::None)
			{
				break;
			}
			if (Result == FJitterBuffer::EPopResult::Packet)
			{
				Decode(Talker, Payload, Now, Stamp);
			}
		}
	}
}

void FSteamVoiceWorker::Decode(FTalker& Talker, TConstArrayView<uint8> Payload, double Now, double Stamp)
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	ISteamUser* User = SteamUser();
	if (!User)
	{
		return;
	}

	uint32 BytesWritten = 0;
	const EVoiceResult Result = User->DecompressVoice(Payload.GetData(), static_cast<uint32>(Payload.Num()), PcmBuffer.GetData(),
		static_cast<uint32>(PcmBuffer.Num() * sizeof(int16)), &BytesWritten, static_cast<uint32>(DecodeSampleRate));
	Talker.LastDecodeResult = static_cast<int32>(Result);
	if (Result != k_EVoiceResultOK)
	{
		++Talker.DecodeErrors;
		Talker.LastDecodedSamples = 0;
		return;
	}

	const int32 NumSamples = FMath::Min(static_cast<int32>(BytesWritten / sizeof(int16)), MaxDecodedSamples);
	Talker.LastDecodedSamples = NumSamples;
	if (NumSamples <= 0)
	{
		return;
	}

	if (Talker.bFake && Stamp > 0.0 && Snapshot->Fake.Count >= 2)
	{
		CheckDecode(Stamp, PcmBuffer.GetData(), NumSamples, Now);
	}

	float SumSquares = 0.0f;
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		const float Sample = static_cast<float>(PcmBuffer[Index]) / 32768.0f;
		FloatBuffer[Index] = Sample;
		SumSquares += Sample * Sample;
	}
	Talker.Level = FMath::Sqrt(SumSquares / static_cast<float>(NumSamples));
	Talker.LastAudio = Now;

	if (Stamp > 0.0)
	{
		const float Latency = static_cast<float>((Now - Stamp) * 1000.0);
		Talker.LatencyMs = Talker.LatencyMs < 0.0f ? Latency : FMath::Lerp(Talker.LatencyMs, Latency, 0.1f);
	}

	Talker.Splitter->PushAudio(FloatBuffer.GetData(), NumSamples);

	if (!Talker.bActive)
	{
		Talker.bActive = true;
		PostEvent(EWorkerEvent::TalkerActive, &Talker, Talker.Id);
	}
#endif
}

void FSteamVoiceWorker::CheckDecode(double Stamp, const int16* Samples, int32 NumSamples, double Now)
{
	uint64 Key = 0;
	FMemory::Memcpy(&Key, &Stamp, sizeof(Key)); // Every fake copy of one captured packet carries the same stamp.

	FDecodeReference* Reference = DecodeReferences.Find(Key);
	if (!Reference)
	{
		if (DecodeReferences.Num() >= 64)
		{
			for (auto It = DecodeReferences.CreateIterator(); It; ++It)
			{
				if (Now - It.Value().Created > DecodeReferenceSeconds)
				{
					It.RemoveCurrent();
				}
			}
		}

		FDecodeReference& Added = DecodeReferences.Add(Key);
		Added.Samples.Append(Samples, NumSamples);
		Added.Created = Now;
		return;
	}

	++Stats.DecodeCompared;
	if (Reference->Samples.Num() != NumSamples)
	{
		++Stats.DecodeLengthMismatch;
	}
	else
	{
		int32 MaxDifference = 0;
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			MaxDifference = FMath::Max(MaxDifference, FMath::Abs(static_cast<int32>(Samples[Index]) - static_cast<int32>(Reference->Samples[Index])));
		}
		Stats.DecodeMaxDifference = FMath::Max(Stats.DecodeMaxDifference, MaxDifference);
		if (MaxDifference == 0)
		{
			++Stats.DecodeIdentical;
		}
	}

	if (++Reference->Seen >= Snapshot->Fake.Count)
	{
		DecodeReferences.Remove(Key);
	}
}

void FSteamVoiceWorker::UpdateActivity(double Now)
{
	for (TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		FTalker& Talker = *Pair.Value;
		const bool bTalking = Talker.LastAudio >= 0.0 && Now - Talker.LastAudio <= TalkingHoldSeconds;
		if (bTalking != Talker.bTalking)
		{
			Talker.bTalking = bTalking;
			if (!bTalking)
			{
				Talker.Level = 0.0f;
			}
			PostEvent(bTalking ? EWorkerEvent::TalkingStarted : EWorkerEvent::TalkingStopped, &Talker, Talker.Id);
		}

		if (Talker.bActive && Now - Talker.LastAudio > IdleSeconds)
		{
			Talker.bActive = false;
			PostEvent(EWorkerEvent::TalkerIdle, &Talker, Talker.Id);
		}
	}

	if (LastCapture >= 0.0 && Now - LastCapture > TalkingHoldSeconds)
	{
		LastCapture = -1.0;
		LocalLevel.store(0.0f, std::memory_order_relaxed);
	}

	const bool bLocal = LastLocalAudio >= 0.0 && Now - LastLocalAudio <= TalkingHoldSeconds;
	if (bLocal != bLocalTalking && Snapshot->LocalId != 0)
	{
		bLocalTalking = bLocal;
		PostEvent(bLocal ? EWorkerEvent::TalkingStarted : EWorkerEvent::TalkingStopped, nullptr, Snapshot->LocalId);
	}
}

// ---- Talkers ----

FSteamVoiceWorker::FTalker& FSteamVoiceWorker::FindOrAddTalker(uint64 Id, bool bFake)
{
	if (TUniquePtr<FTalker>* Found = Talkers.Find(Id))
	{
		return **Found;
	}

	TUniquePtr<FTalker>& Added = Talkers.Add(Id, MakeUnique<FTalker>());
	Added->Id = Id;
	Added->bFake = bFake;
	Added->Limiter.Configure(Config.PacketsPerSecond, Config.BytesPerSecond);
	Added->Splitter = MakeShared<Audio::FPatchSplitter, ESPMode::ThreadSafe>();
	PostEvent(EWorkerEvent::TalkerAdded, Added.Get(), Id);
	return *Added;
}

void FSteamVoiceWorker::RemoveTalker(uint64 Id)
{
	TUniquePtr<FTalker> Removed;
	if (TUniquePtr<FTalker>* Found = Talkers.Find(Id))
	{
		Removed = MoveTemp(*Found);
		Talkers.Remove(Id);
	}
	if (!Removed.IsValid())
	{
		return;
	}

	if (Removed->bTalking)
	{
		PostEvent(EWorkerEvent::TalkingStopped, Removed.Get(), Id);
	}
	PostEvent(EWorkerEvent::TalkerRemoved, Removed.Get(), Id);
}

void FSteamVoiceWorker::PostEvent(EWorkerEvent Type, const FTalker* Talker, uint64 Id)
{
	FWorkerEvent Event;
	Event.Type = Type;
	Event.Talker = Id;
	Event.bFake = Talker && Talker->bFake;
	Event.SampleRate = DecodeSampleRate;
	if (Type == EWorkerEvent::TalkerAdded && Talker)
	{
		Event.Splitter = Talker->Splitter;
	}
	Events.Enqueue(MoveTemp(Event));
}

void FSteamVoiceWorker::CloseAll()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (ISteamUser* User = SteamUser())
	{
		if (bRecording)
		{
			User->StopVoiceRecording();
		}
	}
	if (bSpeakingFlagSet && Snapshot.IsValid())
	{
		if (ISteamFriends* Friends = SteamFriends())
		{
			Friends->SetInGameVoiceSpeaking(CSteamID(Snapshot->LocalId), false);
		}
	}
#endif
	bRecording = false;
	bDrainingTail = false;
	bSpeakingFlagSet = false;

	if (Snapshot.IsValid())
	{
		for (const uint64 Peer : Snapshot->Peers)
		{
			Transport->CloseSession(Peer);
		}
	}
	for (const TPair<uint64, double>& Pending : PendingSessions)
	{
		Transport->CloseSession(Pending.Key);
	}
	PendingSessions.Reset();
	DelayedPackets.Reset();
	DecodeReferences.Reset();
	LocalLevel.store(0.0f, std::memory_order_relaxed);
}

// ---- Stats ----

void FSteamVoiceWorker::PublishStats(double /*Now*/)
{
	Stats.GatedChunks = Gate.GetHeld();
	Stats.ActivationThreshold = ActivationThreshold.load(std::memory_order_relaxed);
	Stats.LoopAverageMs = LoopCount > 0 ? static_cast<float>(LoopTotalMs / static_cast<double>(LoopCount)) : 0.0f;
	Stats.Talkers.Reset();
	for (const TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		const FTalker& Talker = *Pair.Value;
		const FJitterBuffer::FStats& Jitter = Talker.Jitter.GetStats();

		FTalkerStats& Out = Stats.Talkers.AddDefaulted_GetRef();
		Out.Talker = Talker.Id;
		Out.bFake = Talker.bFake;
		Out.bActive = Talker.bActive;
		Out.bTalking = Talker.bTalking;
		Out.Packets = Jitter.Received;
		Out.Lost = Jitter.Lost;
		Out.Late = Jitter.Late;
		Out.Duplicate = Jitter.Duplicate;
		Out.Resyncs = Jitter.Resyncs;
		Out.FloodDrops = Talker.FloodDrops;
		Out.DecodeErrors = Talker.DecodeErrors;
		Out.LastDecodeResult = Talker.LastDecodeResult;
		Out.LastDecodedSamples = Talker.LastDecodedSamples;
		Out.bFlooded = Talker.bFlooded;
		Out.Queued = Talker.Jitter.GetQueued();
		Out.Level = Talker.Level;
		Out.PipelineLatencyMs = Talker.LatencyMs;
	}

	FScopeLock Lock(&StatsLock);
	PublishedStats = Stats;
}
