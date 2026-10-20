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
	/** DecompressVoice supports 11025 to 48000 Hz. */
	constexpr int32 MinDecodeSampleRate = 11025;
	constexpr int32 MaxDecodeSampleRate = 48000;
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
	DecodeSampleRate = Wanted > 0 ? FMath::Clamp(Wanted, MinDecodeSampleRate, MaxDecodeSampleRate) : MaxDecodeSampleRate;

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

float FSteamVoiceWorker::GetTalkerLevel(uint64 Talker) const
{
	FScopeLock Lock(&LevelsLock);
	const float* Level = PublishedLevels.Find(Talker);
	return Level ? *Level : 0.0f;
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
	PublishLevels();
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
	MixChannels(Now);
	UpdateActivity(Now);
	PublishLevels();

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
	const bool bChannelsChanged = !Snapshot.IsValid() || Snapshot->ChannelOutputs != NewSnapshot->ChannelOutputs;
	Snapshot = MoveTemp(NewSnapshot);

	if (bChannelsChanged)
	{
		ApplyChannelOutputs();
	}

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
		const bool bKeep = Talker.Id == MicTestTalkerId ? Snapshot->bMicTest
			: Talker.bFake ? (Talker.Id - FakeTalkerIdBase) < static_cast<uint64>(Snapshot->Fake.Count)
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

void FSteamVoiceWorker::ApplyChannelOutputs()
{
	TMap<int32, TArray<const FChannelOutput*, TInlineAllocator<4>>> Wanted;
	for (const FChannelOutput& Output : Snapshot->ChannelOutputs)
	{
		if (Output.Channel >= 0 && Output.Splitter.IsValid())
		{
			Wanted.FindOrAdd(Output.Channel).Add(&Output);
		}
	}

	// Channels nobody plays any more: the game thread already disconnected their speakers.
	for (auto It = Channels.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	// A channel that stays keeps its mixer, so a speaker added or removed mid-sentence does not cut the others.
	for (const TPair<int32, TArray<const FChannelOutput*, TInlineAllocator<4>>>& Pair : Wanted)
	{
		TUniquePtr<FChannel>& Channel = Channels.FindOrAdd(Pair.Key);
		if (!Channel.IsValid())
		{
			Channel = MakeUnique<FChannel>();
			Channel->Mixer.Configure(DecodeSampleRate, Config.ChannelLeadSeconds);
		}

		TArray<uint64, TInlineAllocator<4>> Excluded;
		for (const FChannelOutput* Output : Pair.Value)
		{
			Excluded.Add(Output->Excluded);
		}

		const TArray<FPatchSplitterPtr> OldSplitters = MoveTemp(Channel->Splitters);
		const TArray<bool> OldActive = MoveTemp(Channel->Active);
		TArray<uint64, TInlineAllocator<4>> OldExcluded;
		for (int32 Index = 0; Index < Channel->Mixer.NumOutputs(); ++Index)
		{
			OldExcluded.Add(Channel->Mixer.GetOutputExcluded(Index));
		}

		Channel->Mixer.SetOutputs(Excluded);
		Channel->Splitters.Reset(Channel->Mixer.NumOutputs());
		Channel->Active.Reset(Channel->Mixer.NumOutputs());
		for (int32 Index = 0; Index < Channel->Mixer.NumOutputs(); ++Index)
		{
			const uint64 Id = Channel->Mixer.GetOutputExcluded(Index);
			const FChannelOutput* const* Output = Pair.Value.FindByPredicate([Id](const FChannelOutput* Candidate) { return Candidate->Excluded == Id; });
			Channel->Splitters.Add((*Output)->Splitter);

			// Still active only for the same splitter: a new one has new speakers that wait for their start event.
			const int32 Old = OldExcluded.IndexOfByKey(Id);
			Channel->Active.Add(Old != INDEX_NONE && OldSplitters.IsValidIndex(Old) && OldSplitters[Old] == (*Output)->Splitter && OldActive[Old]);
		}
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

	if (Snapshot->bMicTest)
	{
		QueueMicTest(Data, Size, Now, bIdMatches, bCrcMatches);
	}
	if (!Snapshot->bSend)
	{
		return; // Only the mic test records.
	}

	// Where our voice goes travels in every packet, so a speaker starts and stops on the right syllable.
	const uint8 Flags = Snapshot->bTransmitMouth ? PacketFlags::Mouth : 0;
	const TConstArrayView<int32> TransmitChannels = Snapshot->TransmitChannels;
	const bool bTransmits = Flags != 0 || !TransmitChannels.IsEmpty();
	if (Snapshot->LocalId != 0)
	{
		UpdateTransmit(LocalTransmit, Snapshot->LocalId, Flags != 0, TransmitChannels);
	}

	const FFakeTalkerConfig& Fake = Snapshot->Fake;
	const int32 FakeChannel = Fake.Channel;
	const bool bFakes = Fake.Count > 0 && (bTransmits || FakeChannel >= 0);
	if (!bTransmits)
	{
		++Stats.NotTransmitted;
		if (!bFakes)
		{
			return;
		}
	}

	const uint16 PacketSequence = Sequence++;
	const TConstArrayView<uint8> Payload(Data, Size);
	if (bTransmits)
	{
		// Each peer gets only what the server rules let them hear (their Listen channels, our mouth in range). Peers with the same
		// route share one written packet; without server rules that is every peer.
		FVoiceRoute Base;
		Base.bMouth = Flags != 0;
		Base.Channels.Append(TransmitChannels.GetData(), TransmitChannels.Num());
		FVoiceRoute Route;
		FVoiceRoute Written;
		bool bWritten = false;
		for (const uint64 Peer : Snapshot->Peers)
		{
			if (Snapshot->Blocked.Contains(Peer))
			{
				continue;
			}
			FilterForListener(Snapshot->FindRules(Peer), !Snapshot->MouthNotSent.Contains(Peer), Base, Route);
			if (Route.IsEmpty())
			{
				++Stats.RuleNotSent;
				continue;
			}
			if (!bWritten || Route != Written)
			{
				if (!WriteVoicePacket(PacketSequence, Route.bMouth ? PacketFlags::Mouth : 0, Route.Channels, Payload, PacketBuffer))
				{
					++Stats.CaptureTooLarge;
					return;
				}
				Written = Route;
				bWritten = true;
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
	}

	// Fake talkers: the same packet comes back later as somebody else, through the full receive path.
	// With a fake channel they talk on that channel only (radio), so Channel speakers can be tested alone.
	for (int32 Index = 0; bFakes && Index < FMath::Min(Fake.Count, MaxFakeTalkers); ++Index)
	{
		if (Fake.LossPercent > 0.0f && Random.FRand() * 100.0f < Fake.LossPercent)
		{
			continue;
		}

		FDelayedPacket& Delayed = DelayedPackets.AddDefaulted_GetRef();
		Delayed.Due = Now + (Fake.DelayMs + Index * Fake.StaggerMs) / 1000.0;
		Delayed.Stamp = Now;
		Delayed.Talker = FakeTalkerIdBase + static_cast<uint64>(Index);
		// The fakes get everything we send (their own receive path applies our Listen rule); a fake channel replaces it.
		const bool bFakeWritten = FakeChannel < 0
			? WriteVoicePacket(PacketSequence, Flags, TransmitChannels, Payload, Delayed.Data)
			: WriteVoicePacket(PacketSequence, 0, TConstArrayView<int32>(&FakeChannel, 1), Payload, Delayed.Data);
		if (!bFakeWritten)
		{
			DelayedPackets.Pop(EAllowShrinking::No);
			continue;
		}

		// Steam decodes per speaker id inside the payload: give each fake its own id, or all fakes would share our decoder.
		// Only when this payload has exactly the expected layout; otherwise the rewrite would damage it.
		const TArrayView<uint8> FakePayload(Delayed.Data.GetData() + Delayed.Data.Num() - Size, Size);
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

void FSteamVoiceWorker::QueueMicTest(const uint8* Data, int32 Size, double Now, bool bIdMatches, bool bCrcMatches)
{
	// Through the full receive path like a fake talker, with no extra delay: the speaker's playout delay is the delay a listener hears.
	FDelayedPacket& Delayed = DelayedPackets.AddDefaulted_GetRef();
	Delayed.Due = Now;
	Delayed.Stamp = Now;
	Delayed.Talker = MicTestTalkerId;

	// It needs its own speaker id: decoding with ours would disturb the decoder state the level measurement uses (and is refused on receive).
	const bool bWritten = bIdMatches && bCrcMatches
		&& WriteVoicePacket(MicTestSequence++, PacketFlags::Mouth, TConstArrayView<int32>(), TConstArrayView<uint8>(Data, Size), Delayed.Data);
	if (!bWritten || !RewritePayloadSpeaker(TArrayView<uint8>(Delayed.Data.GetData() + Delayed.Data.Num() - Size, Size), MicTestTalkerId))
	{
		++Stats.FakeRewriteFailed;
		DelayedPackets.Pop(EAllowShrinking::No);
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

	// The server's rules: a server-muted or refused talker is dropped by everyone, and a refused local player accepts nothing.
	const FPlayerRules& SenderRules = Snapshot->FindRules(Sender);
	if (!SenderRules.bCanTalk || SenderRules.bRefused || Snapshot->LocalRules.bRefused)
	{
		++Stats.RuleDrops;
		return;
	}

	// A muted talker whose talking is still reported is queued like anybody else and skipped at playout (never decoded).
	if (Snapshot->Blocked.Contains(Sender) || (Snapshot->Muted.Contains(Sender) && !Snapshot->bReportMutedTalking))
	{
		++Stats.MutedDrops;
		return;
	}

	if (ReadPayloadSpeaker(TConstArrayView<uint8>(Packet.Payload, Packet.PayloadSize)) != Sender)
	{
		// Steam decodes per speaker id inside the payload: a packet carrying somebody else's id would drive that player's decoder state.
		++Stats.SpeakerMismatch;
		return;
	}

	// The whole packet is queued: where it plays (mouth, channels) is decided when it is played, against the routing of that moment.
	Talker.Jitter.Push(Packet.Sequence, TConstArrayView<uint8>(Data, Size), Now, Stamp);
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
	FVoiceRoute Allowed;
	FVoiceRoute Route;
	for (TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		FTalker& Talker = *Pair.Value;
		for (;;)
		{
			TConstArrayView<uint8> Queued;
			double Stamp = 0.0;
			const FJitterBuffer::EPopResult Result = Talker.Jitter.Pop(Now, MaxGapWaitSeconds, Queued, Stamp);
			if (Result == FJitterBuffer::EPopResult::None)
			{
				break;
			}

			FPacketView Packet;
			if (Result != FJitterBuffer::EPopResult::Packet || ReadPacket(Queued.GetData(), Queued.Num(), Packet) != EPacketReadResult::Voice)
			{
				continue;
			}

			// The server rules as they are now apply to queued packets too: the talker's (server mute, owner voice, transmit channels),
			// then ours (listen channels, the talker's mouth out of range). Nothing left: dropped, and the talker is not reported talking.
			FilterForTalker(Snapshot->FindRules(Talker.Id), Packet.HasMouth(), Packet.GetChannels(), Allowed);
			FilterForListener(Snapshot->LocalRules, !Snapshot->MouthDropped.Contains(Talker.Id), Allowed, Route);
			if (Route.IsEmpty())
			{
				++Stats.RuleDrops;
				continue;
			}
			if (Packet.HasMouth() && !Route.bMouth)
			{
				++Stats.RuleMouthRemoved;
			}
			if (Route.Channels.Num() != Packet.NumChannels)
			{
				++Stats.RuleChannelsRemoved;
			}

			// A mute set while packets were queued applies to them too.
			const bool bMuted = Snapshot->Muted.Contains(Talker.Id);
			if (Snapshot->Blocked.Contains(Talker.Id) || (bMuted && !Snapshot->bReportMutedTalking))
			{
				++Stats.MutedDrops;
				continue;
			}

			// Talking and where the voice goes follow the played packets, whether or not a speaker here plays them.
			Talker.LastAudio = Now;
			UpdateTransmit(Talker.Transmit, Talker.Id, Route.bMouth, Route.Channels);
			if (bMuted)
			{
				++Stats.MutedDrops;
				continue;
			}
			Decode(Talker, Packet, Route, Now, Stamp);
		}
	}
}

void FSteamVoiceWorker::Decode(FTalker& Talker, const FPacketView& Packet, const FVoiceRoute& Route, double Now, double Stamp)
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	ISteamUser* User = SteamUser();
	if (!User)
	{
		return;
	}

	// The channels of this packet somebody here plays. Nothing to play: no decode (Steam's decoder treats the gap like a lost packet).
	TArray<FChannel*, TInlineAllocator<MaxPacketChannels>> Targets;
	for (const int32 Id : Route.Channels)
	{
		if (const TUniquePtr<FChannel>* Found = Channels.Find(Id))
		{
			Targets.AddUnique(Found->Get());
		}
	}
	if (!Route.bMouth && Targets.IsEmpty())
	{
		++Stats.NotPlayed;
		return;
	}

	uint32 BytesWritten = 0;
	const EVoiceResult Result = User->DecompressVoice(Packet.Payload, static_cast<uint32>(Packet.PayloadSize), PcmBuffer.GetData(),
		static_cast<uint32>(PcmBuffer.Num() * sizeof(int16)), &BytesWritten, static_cast<uint32>(DecodeSampleRate));
	Talker.LastDecodeResult = static_cast<int32>(Result);
	if (Result != k_EVoiceResultOK)
	{
		++Talker.DecodeErrors;
		Talker.LastDecodedSamples = 0;
		return;
	}

	int32 NumSamples = FMath::Min(static_cast<int32>(BytesWritten / sizeof(int16)), MaxDecodedSamples);
	Talker.LastDecodedSamples = NumSamples;
	if (NumSamples <= 0)
	{
		return;
	}

	if (Talker.bFake && Stamp > 0.0 && Snapshot->Fake.Count >= 2)
	{
		CheckDecode(Stamp, PcmBuffer.GetData(), NumSamples, Now);
	}
	Talker.MaxDecodedSamples = FMath::Max(Talker.MaxDecodedSamples, NumSamples);

	// Steam sends nothing during a pause and decodes its length as zeros at the start of the next packet. The speaker already waited
	// through that time (no audio arrived), so the part of the zeros it covers is dropped; otherwise every pause adds its length to the delay.
	int32 First = 0;
	const double Uncovered = Talker.AudioEnd < 0.0 ? TNumericLimits<double>::Max() : Now - Talker.AudioEnd;
	if (Uncovered > 0.0)
	{
		int32 LeadingZeros = 0;
		while (LeadingZeros < NumSamples && PcmBuffer[LeadingZeros] == 0)
		{
			++LeadingZeros;
		}
		First = static_cast<int32>(FMath::Min(Uncovered * static_cast<double>(DecodeSampleRate), static_cast<double>(LeadingZeros)));
		Talker.SilenceTrimmed += static_cast<uint64>(First);
	}
	Talker.AudioEnd = FMath::Max(Talker.AudioEnd, Now) + static_cast<double>(NumSamples - First) / static_cast<double>(DecodeSampleRate);

	const int16* Pcm = PcmBuffer.GetData() + First;
	NumSamples -= First;
	if (NumSamples <= 0)
	{
		return;
	}

	float SumSquares = 0.0f;
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		const float Sample = static_cast<float>(Pcm[Index]) / 32768.0f;
		FloatBuffer[Index] = Sample;
		SumSquares += Sample * Sample;
	}
	Talker.Level = FMath::Sqrt(SumSquares / static_cast<float>(NumSamples));
	bLevelsDirty = true;

	if (Stamp > 0.0)
	{
		const float Latency = static_cast<float>((Now - Stamp) * 1000.0);
		Talker.LatencyMs = Talker.LatencyMs < 0.0f ? Latency : FMath::Lerp(Talker.LatencyMs, Latency, 0.1f);
	}

	// Player volume, before the mouth and the channel mixes (the level above stays the talker's own). A change ramps over this packet, no click.
	const float* Volume = Snapshot->PlayerVolumes.Find(Talker.Id);
	const float TargetGain = Volume ? *Volume : 1.0f;
	const float StartGain = Talker.Gain < 0.0f ? TargetGain : Talker.Gain;
	Talker.Gain = TargetGain;
	if (StartGain != 1.0f || TargetGain != 1.0f)
	{
		const float GainStep = (TargetGain - StartGain) / static_cast<float>(NumSamples);
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			FloatBuffer[Index] = FMath::Clamp(FloatBuffer[Index] * (StartGain + GainStep * static_cast<float>(Index + 1)), -1.0f, 1.0f);
		}
	}

	if (Route.bMouth)
	{
		Talker.LastMouthAudio = Now;
		Talker.Splitter->PushAudio(FloatBuffer.GetData(), NumSamples);
		if (!Talker.bActive)
		{
			Talker.bActive = true;
			PostEvent(EWorkerEvent::TalkerActive, &Talker, Talker.Id);
		}
	}

	for (FChannel* Channel : Targets)
	{
		Channel->Mixer.Add(Talker.Id, FloatBuffer.GetData(), NumSamples, Now);
	}
#endif
}

void FSteamVoiceWorker::MixChannels(double Now)
{
	for (TPair<int32, TUniquePtr<FChannel>>& Pair : Channels)
	{
		FChannel& Channel = *Pair.Value;
		Channel.Mixer.Advance(Now, [&Channel](int32 Output, TConstArrayView<float> Samples)
		{
			Channel.Splitters[Output]->PushAudio(Samples.GetData(), Samples.Num());
		});
	}
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
				bLevelsDirty = true;
				UpdateTransmit(Talker.Transmit, Talker.Id, false, TConstArrayView<int32>());
			}
			PostEvent(bTalking ? EWorkerEvent::TalkingStarted : EWorkerEvent::TalkingStopped, &Talker, Talker.Id);
		}

		if (Talker.bActive && Now - Talker.LastMouthAudio > IdleSeconds)
		{
			Talker.bActive = false;
			PostEvent(EWorkerEvent::TalkerIdle, &Talker, Talker.Id);
		}
	}

	for (TPair<int32, TUniquePtr<FChannel>>& Pair : Channels)
	{
		FChannel& Channel = *Pair.Value;
		for (int32 Output = 0; Output < Channel.Active.Num(); ++Output)
		{
			const double LastAudio = Channel.Mixer.GetOutputLastAudio(Output);
			const bool bActive = LastAudio >= 0.0 && Now - LastAudio <= IdleSeconds;
			if (bActive != Channel.Active[Output])
			{
				Channel.Active[Output] = bActive;
				PostChannelEvent(bActive ? EWorkerEvent::ChannelActive : EWorkerEvent::ChannelIdle, Pair.Key, Channel.Mixer.GetOutputExcluded(Output), Channel.Splitters[Output]);
			}
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
		if (!bLocal)
		{
			UpdateTransmit(LocalTransmit, Snapshot->LocalId, false, TConstArrayView<int32>());
		}
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
	bLevelsDirty = true;

	for (TPair<int32, TUniquePtr<FChannel>>& Pair : Channels)
	{
		Pair.Value->Mixer.RemoveTalker(Id);
	}

	UpdateTransmit(Removed->Transmit, Id, false, TConstArrayView<int32>());
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

void FSteamVoiceWorker::PostChannelEvent(EWorkerEvent Type, int32 Channel, uint64 Excluded, const FPatchSplitterPtr& Splitter)
{
	FWorkerEvent Event;
	Event.Type = Type;
	Event.Talker = Excluded;
	Event.Channel = Channel;
	Event.SampleRate = DecodeSampleRate;
	Event.Splitter = Splitter;
	Events.Enqueue(MoveTemp(Event));
}

void FSteamVoiceWorker::UpdateTransmit(FTransmitState& State, uint64 Id, bool bMouth, TConstArrayView<int32> INChannels)
{
	if (!State.Set(bMouth, INChannels))
	{
		return;
	}

	FWorkerEvent Event;
	Event.Type = EWorkerEvent::TransmitChanged;
	Event.Talker = Id;
	Event.bMouth = State.bMouth;
	Event.Channels.Append(State.Channels.GetData(), State.Channels.Num());
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
	Channels.Reset();
	DecodeReferences.Reset();
	LocalLevel.store(0.0f, std::memory_order_relaxed);
	for (TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		Pair.Value->Level = 0.0f;
	}
	bLevelsDirty = true;
}

// ---- Stats ----

void FSteamVoiceWorker::PublishLevels()
{
	if (!bLevelsDirty)
	{
		return;
	}
	bLevelsDirty = false;

	FScopeLock Lock(&LevelsLock);
	PublishedLevels.Reset();
	for (const TPair<uint64, TUniquePtr<FTalker>>& Pair : Talkers)
	{
		if (Pair.Value->Level > 0.0f)
		{
			PublishedLevels.Add(Pair.Key, Pair.Value->Level);
		}
	}
}

void FSteamVoiceWorker::PublishStats(double Now)
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
		Out.MaxPacketMs = 1000.0f * static_cast<float>(Talker.MaxDecodedSamples) / static_cast<float>(DecodeSampleRate);
		Out.SilenceTrimmedMs = static_cast<float>(1000.0 * static_cast<double>(Talker.SilenceTrimmed) / static_cast<double>(DecodeSampleRate));
		Out.bFlooded = Talker.bFlooded;
		Out.Queued = Talker.Jitter.GetQueued();
		Out.Level = Talker.Level;
		Out.Gain = Talker.Gain < 0.0f ? 1.0f : Talker.Gain;
		Out.PipelineLatencyMs = Talker.LatencyMs;
	}

	Stats.Channels.Reset();
	for (const TPair<int32, TUniquePtr<FChannel>>& Pair : Channels)
	{
		const FChannel& Channel = *Pair.Value;
		for (int32 Output = 0; Output < Channel.Mixer.NumOutputs(); ++Output)
		{
			const FChannelMixer::FOutputStats Mix = Channel.Mixer.GetOutputStats(Output, Now);
			FChannelOutputStats& Out = Stats.Channels.AddDefaulted_GetRef();
			Out.Channel = Pair.Key;
			Out.Excluded = Mix.Excluded;
			Out.bRunning = Channel.Mixer.IsRunning();
			Out.bActive = Channel.Active.IsValidIndex(Output) && Channel.Active[Output];
			Out.ActiveTalkers = Mix.ActiveTalkers;
			Out.Gain = Mix.Gain;
			Out.Limited = Mix.Limited;
			Out.OverflowSamples = Channel.Mixer.GetOverflowSamples();
		}
	}

	FScopeLock Lock(&StatsLock);
	PublishedStats = Stats;
}
