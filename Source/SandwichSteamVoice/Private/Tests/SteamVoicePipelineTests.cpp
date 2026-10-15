// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SteamEngineCompat.h"
#include "Math/RandomStream.h"
#include "SteamVoiceActivationGate.h"
#include "SteamVoiceChannelMixer.h"
#include "SteamVoiceJitterBuffer.h"
#include "SteamVoicePacket.h"
#include "SteamVoiceRateLimiter.h"

using namespace SandwichSteam::Voice;

namespace
{
	TArray<uint8> MakePayload(int32 Size, uint8 Seed)
	{
		TArray<uint8> Payload;
		Payload.SetNumUninitialized(Size);
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Payload[Index] = static_cast<uint8>(Seed + Index);
		}
		return Payload;
	}

	/** Pops everything that is ready and returns the first payload byte of each played packet (-1 for a lost one). */
	TArray<int32> Drain(FJitterBuffer& Buffer, double Now, double MaxWait = 0.06)
	{
		TArray<int32> Played;
		for (;;)
		{
			TConstArrayView<uint8> Payload;
			double Stamp = 0.0;
			const FJitterBuffer::EPopResult Result = Buffer.Pop(Now, MaxWait, Payload, Stamp);
			if (Result == FJitterBuffer::EPopResult::None)
			{
				break;
			}
			Played.Add(Result == FJitterBuffer::EPopResult::Packet ? Payload[0] : -1);
		}
		return Played;
	}

	void PushOne(FJitterBuffer& Buffer, uint16 Sequence, double Now)
	{
		const uint8 Marker = static_cast<uint8>(Sequence & 0xFF);
		Buffer.Push(Sequence, TConstArrayView<uint8>(&Marker, 1), Now);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoicePacketRoundTripTest, "SandwichSteam.Voice.Pipeline.PacketRoundTrip",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoicePacketRoundTripTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Payload = MakePayload(200, 7);
	const int32 Channels[] = { 0, 3, 2147483647 };
	TArray<uint8> Bytes;

	TestTrue(TEXT("Voice packet written"), WriteVoicePacket(65535, PacketFlags::Mouth, Channels, Payload, Bytes));
	TestEqual(TEXT("Size = header + channels + payload"), Bytes.Num(), PacketHeaderBytes + 3 * 4 + 200);

	FPacketView View;
	TestTrue(TEXT("Read as voice"), ReadPacket(Bytes.GetData(), Bytes.Num(), View) == EPacketReadResult::Voice);
	TestEqual(TEXT("Sequence"), static_cast<int32>(View.Sequence), 65535);
	TestTrue(TEXT("Mouth flag"), View.HasMouth());
	TestTrue(TEXT("Channels"), View.NumChannels == 3 && View.Channels[0] == 0 && View.Channels[1] == 3 && View.Channels[2] == 2147483647);
	TestTrue(TEXT("Payload"), View.PayloadSize == 200 && FMemory::Memcmp(View.Payload, Payload.GetData(), 200) == 0);

	// No channels, no mouth (a channel-only packet still carries a payload).
	TestTrue(TEXT("Packet without channels"), WriteVoicePacket(1, 0, TConstArrayView<int32>(), Payload, Bytes));
	TestTrue(TEXT("Read"), ReadPacket(Bytes.GetData(), Bytes.Num(), View) == EPacketReadResult::Voice);
	TestFalse(TEXT("No mouth"), View.HasMouth());

	// Hello.
	WriteHelloPacket(Bytes);
	TestEqual(TEXT("Hello is 4 bytes"), Bytes.Num(), HelloPacketBytes);
	TestTrue(TEXT("Read as hello"), ReadPacket(Bytes.GetData(), Bytes.Num(), View) == EPacketReadResult::Hello);

	// A voice packet never carries the hello flag.
	TestTrue(TEXT("Hello flag is stripped"), WriteVoicePacket(1, PacketFlags::Hello | PacketFlags::Mouth, TConstArrayView<int32>(), Payload, Bytes)
		&& ReadPacket(Bytes.GetData(), Bytes.Num(), View) == EPacketReadResult::Voice);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoicePacketValidationTest, "SandwichSteam.Voice.Pipeline.PacketValidation",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoicePacketValidationTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Bytes;
	const TArray<uint8> Payload = MakePayload(10, 1);

	// Writer refuses invalid packets.
	const int32 TooMany[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
	TestFalse(TEXT("Nine channels refused"), WriteVoicePacket(0, 0, TooMany, Payload, Bytes));
	const int32 Negative[] = { -1 };
	TestFalse(TEXT("Negative channel refused"), WriteVoicePacket(0, 0, Negative, Payload, Bytes));
	TestFalse(TEXT("Empty payload refused"), WriteVoicePacket(0, 0, TConstArrayView<int32>(), TConstArrayView<uint8>(), Bytes));
	TestFalse(TEXT("Oversized refused"), WriteVoicePacket(0, 0, TConstArrayView<int32>(), MakePayload(MaxPacketBytes, 0), Bytes));
	TestTrue(TEXT("Largest allowed"), WriteVoicePacket(0, 0, TConstArrayView<int32>(), MakePayload(MaxPacketBytes - PacketHeaderBytes, 0), Bytes));

	// Reader refuses invalid packets.
	FPacketView View;
	const uint8 Short[] = { 1, 2, 3 };
	TestTrue(TEXT("Too small"), ReadPacket(Short, 3, View) == EPacketReadResult::TooSmall);
	TestTrue(TEXT("Null"), ReadPacket(nullptr, 10, View) == EPacketReadResult::TooSmall);

	TArray<uint8> Large = MakePayload(MaxPacketBytes + 1, 0);
	Large[2] = 0;
	Large[3] = 0;
	TestTrue(TEXT("Too large"), ReadPacket(Large.GetData(), Large.Num(), View) == EPacketReadResult::TooLarge);

	const uint8 NineChannels[] = { 0, 0, 0, 9, 0, 0, 0, 0 };
	TestTrue(TEXT("Too many channels"), ReadPacket(NineChannels, sizeof(NineChannels), View) == EPacketReadResult::TooManyChannels);

	const uint8 HeaderOnly[] = { 0, 0, 1, 0 };
	TestTrue(TEXT("No payload"), ReadPacket(HeaderOnly, sizeof(HeaderOnly), View) == EPacketReadResult::NoPayload);

	const uint8 ChannelsNoPayload[] = { 0, 0, 1, 1, 5, 0, 0, 0 };
	TestTrue(TEXT("Channel list without payload"), ReadPacket(ChannelsNoPayload, sizeof(ChannelsNoPayload), View) == EPacketReadResult::NoPayload);

	const uint8 ChannelCut[] = { 0, 0, 1, 2, 5, 0, 0, 0, 9 };
	TestTrue(TEXT("Channel list longer than the packet"), ReadPacket(ChannelCut, sizeof(ChannelCut), View) == EPacketReadResult::NoPayload);

	const uint8 NegativeChannel[] = { 0, 0, 1, 1, 0xFF, 0xFF, 0xFF, 0xFF, 42 };
	TestTrue(TEXT("Negative channel"), ReadPacket(NegativeChannel, sizeof(NegativeChannel), View) == EPacketReadResult::InvalidChannelId);

	const uint8 LongHello[] = { 0, 0, PacketFlags::Hello, 0, 1 };
	TestTrue(TEXT("Hello with data"), ReadPacket(LongHello, sizeof(LongHello), View) == EPacketReadResult::MalformedHello);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoicePacketFuzzTest, "SandwichSteam.Voice.Pipeline.PacketFuzz",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoicePacketFuzzTest::RunTest(const FString& Parameters)
{
	// Random bytes of random length: the reader must never read outside the buffer, and whatever it accepts must be self-consistent.
	FRandomStream Random(1234);
	TArray<uint8> Bytes;
	int32 Inconsistent = 0;
	int32 Accepted = 0;
	for (int32 Iteration = 0; Iteration < 20000; ++Iteration)
	{
		const int32 Size = Random.RandRange(0, MaxPacketBytes + 16);
		Bytes.SetNumUninitialized(Size);
		for (uint8& Byte : Bytes)
		{
			Byte = static_cast<uint8>(Random.RandRange(0, 255));
		}
		// Bias toward plausible headers so the deeper checks run too.
		if (Size >= 4 && Random.FRand() < 0.5f)
		{
			Bytes[2] &= PacketFlags::Mouth;
			Bytes[3] = static_cast<uint8>(Random.RandRange(0, 9));
		}

		FPacketView View;
		const EPacketReadResult Result = ReadPacket(Bytes.GetData(), Bytes.Num(), View);
		if (Result == EPacketReadResult::Voice)
		{
			++Accepted;
			const bool bInside = View.Payload >= Bytes.GetData() && View.Payload + View.PayloadSize == Bytes.GetData() + Size;
			const bool bConsistent = View.PayloadSize > 0 && View.NumChannels <= MaxPacketChannels
				&& PacketHeaderBytes + 4 * View.NumChannels + View.PayloadSize == Size;
			bool bChannelsValid = true;
			for (const int32 Channel : View.GetChannels())
			{
				bChannelsValid &= Channel >= 0;
			}
			Inconsistent += (bInside && bConsistent && bChannelsValid) ? 0 : 1;
		}
	}
	TestEqual(TEXT("Accepted packets are consistent"), Inconsistent, 0);
	TestTrue(TEXT("Some packets were accepted (the fuzz reached the deep checks)"), Accepted > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceSequenceTest, "SandwichSteam.Voice.Pipeline.Sequence",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceSequenceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Newer"), SequenceDelta(5, 3), 2);
	TestEqual(TEXT("Older"), SequenceDelta(3, 5), -2);
	TestEqual(TEXT("Wrap forward"), SequenceDelta(1, 65535), 2);
	TestEqual(TEXT("Wrap backward"), SequenceDelta(65535, 1), -2);
	TestEqual(TEXT("Same"), SequenceDelta(100, 100), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoicePayloadSpeakerTest, "SandwichSteam.Voice.Pipeline.PayloadSpeaker",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoicePayloadSpeakerTest::RunTest(const FString& Parameters)
{
	// Standard CRC-32 check value.
	const char* Check = "123456789";
	TestTrue(TEXT("CRC-32 of 123456789"), Crc32(reinterpret_cast<const uint8*>(Check), 9) == 0xCBF43926u);

	// A payload shaped like Steam's: id, body, CRC.
	TArray<uint8> Payload = MakePayload(40, 3);
	TestTrue(TEXT("Rewrite works on a long enough payload"), RewritePayloadSpeaker(Payload, 76561197988649870ull));
	TestTrue(TEXT("Speaker read back"), ReadPayloadSpeaker(Payload) == 76561197988649870ull);
	TestTrue(TEXT("CRC valid after the rewrite"), HasValidPayloadCrc(Payload));

	Payload[20] ^= 0xFF;
	TestFalse(TEXT("A changed body breaks the CRC"), HasValidPayloadCrc(Payload));

	TArray<uint8> Short = MakePayload(SteamPayloadMinBytes - 1, 0);
	TestFalse(TEXT("Too short to rewrite"), RewritePayloadSpeaker(Short, 1));
	TestTrue(TEXT("Too short has no speaker"), ReadPayloadSpeaker(Short) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceRateLimiterTest, "SandwichSteam.Voice.Pipeline.RateLimiter",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceRateLimiterTest::RunTest(const FString& Parameters)
{
	FRateLimiter Limiter;
	Limiter.Configure(10.0f, 1000.0f);

	// One second of burst, then refused.
	int32 Allowed = 0;
	for (int32 Index = 0; Index < 15; ++Index)
	{
		Allowed += Limiter.TryConsume(10, 0.0) ? 1 : 0;
	}
	TestEqual(TEXT("Packet bucket holds one second"), Allowed, 10);
	TestEqual(TEXT("Refusals counted"), static_cast<int32>(Limiter.GetDropped()), 5);

	// Refill over time.
	TestTrue(TEXT("Refilled after 0.1 s"), Limiter.TryConsume(10, 0.1));

	// Byte limit.
	Limiter.Configure(100.0f, 1000.0f);
	TestTrue(TEXT("Big packet fits the byte bucket"), Limiter.TryConsume(900, 0.0));
	TestFalse(TEXT("Next big packet does not"), Limiter.TryConsume(200, 0.0));
	TestTrue(TEXT("A small one still does"), Limiter.TryConsume(100, 0.0));

	// Flood: refused packets for 5 s in a row.
	Limiter.Configure(1.0f, 100000.0f);
	double Now = 0.0;
	for (; Now < 4.9; Now += 0.05)
	{
		Limiter.TryConsume(1, Now);
	}
	TestFalse(TEXT("Not flooding before 5 s"), Limiter.IsFlooding());
	for (; Now < 5.3; Now += 0.05)
	{
		Limiter.TryConsume(1, Now);
	}
	TestTrue(TEXT("Flooding after 5 s over the limit"), Limiter.IsFlooding());

	// A quiet pause resets the streak.
	Limiter.Configure(1.0f, 100000.0f);
	for (Now = 0.0; Now < 3.0; Now += 0.05)
	{
		Limiter.TryConsume(1, Now);
	}
	for (Now = 4.5; Now < 7.0; Now += 0.05)
	{
		Limiter.TryConsume(1, Now);
	}
	TestFalse(TEXT("A pause over 1 s starts a new streak"), Limiter.IsFlooding());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceJitterBufferTest, "SandwichSteam.Voice.Pipeline.JitterBuffer",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceJitterBufferTest::RunTest(const FString& Parameters)
{
	// In order.
	{
		FJitterBuffer Buffer;
		PushOne(Buffer, 10, 0.0);
		PushOne(Buffer, 11, 0.0);
		TestTrue(TEXT("In order plays at once"), Drain(Buffer, 0.0) == TArray<int32>({ 10, 11 }));
	}

	// Reordered within the wait: played in order, nothing lost.
	{
		FJitterBuffer Buffer;
		PushOne(Buffer, 10, 0.0);
		TestTrue(TEXT("First"), Drain(Buffer, 0.0) == TArray<int32>({ 10 }));
		PushOne(Buffer, 12, 0.01);
		TestEqual(TEXT("Waits for the gap"), Drain(Buffer, 0.02).Num(), 0);
		PushOne(Buffer, 11, 0.03);
		TestTrue(TEXT("Gap filled: both in order"), Drain(Buffer, 0.03) == TArray<int32>({ 11, 12 }));
		TestEqual(TEXT("Nothing lost"), static_cast<int32>(Buffer.GetStats().Lost), 0);
	}

	// Loss: skipped after the wait.
	{
		FJitterBuffer Buffer;
		PushOne(Buffer, 10, 0.0);
		Drain(Buffer, 0.0);
		PushOne(Buffer, 12, 0.01);
		TestEqual(TEXT("Still waiting at 50 ms"), Drain(Buffer, 0.05).Num(), 0);
		TestTrue(TEXT("Skipped after 60 ms"), Drain(Buffer, 0.08) == TArray<int32>({ -1, 12 }));
		TestEqual(TEXT("Lost counted"), static_cast<int32>(Buffer.GetStats().Lost), 1);

		// The lost one arrives after all: late.
		TestTrue(TEXT("Late packet refused"), Buffer.Push(11, MakePayload(1, 11), 0.09) == FJitterBuffer::EPushResult::Late);
	}

	// Duplicates.
	{
		FJitterBuffer Buffer;
		PushOne(Buffer, 5, 0.0);
		TestTrue(TEXT("Duplicate refused"), Buffer.Push(5, MakePayload(1, 5), 0.0) == FJitterBuffer::EPushResult::Duplicate);
		TestTrue(TEXT("Played once"), Drain(Buffer, 0.0) == TArray<int32>({ 5 }));
	}

	// Sequence wrap.
	{
		FJitterBuffer Buffer;
		PushOne(Buffer, 65534, 0.0);
		PushOne(Buffer, 0, 0.0);
		PushOne(Buffer, 65535, 0.0);
		TestTrue(TEXT("Wraps in order"), Drain(Buffer, 0.0) == TArray<int32>({ 254, 255, 0 }));
	}

	// A jump far ahead (talker restarted): resync instead of waiting for hundreds of packets.
	{
		FJitterBuffer Buffer;
		PushOne(Buffer, 100, 0.0);
		Drain(Buffer, 0.0);
		TestTrue(TEXT("Far jump resyncs"), Buffer.Push(100 + FJitterBuffer::Capacity + 5, MakePayload(1, 9), 0.1) == FJitterBuffer::EPushResult::Resync);
		TestTrue(TEXT("Plays at once after resync"), Drain(Buffer, 0.1) == TArray<int32>({ 9 }));
	}

	// Stamps are carried.
	{
		FJitterBuffer Buffer;
		const uint8 Byte = 1;
		Buffer.Push(1, TConstArrayView<uint8>(&Byte, 1), 0.5, 0.25);
		TConstArrayView<uint8> Payload;
		double Stamp = 0.0;
		TestTrue(TEXT("Popped"), Buffer.Pop(0.5, 0.06, Payload, Stamp) == FJitterBuffer::EPopResult::Packet);
		TestEqual(TEXT("Stamp carried"), Stamp, 0.25);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceActivationGateTest, "SandwichSteam.Voice.Pipeline.ActivationGate",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceActivationGateTest::RunTest(const FString& Parameters)
{
	using EDecision = FActivationGate::EDecision;

	FActivationGate Gate;
	Gate.Configure(0.3);

	// Off: everything is sent, also silence.
	TestTrue(TEXT("Threshold 0 sends silence"), Gate.Evaluate(0.0f, 0.0) == EDecision::Send);

	Gate.SetThreshold(0.05f);
	TestTrue(TEXT("Quiet packet is held"), Gate.Evaluate(0.01f, 1.0) == EDecision::Hold);
	TestTrue(TEXT("Unknown level is sent"), Gate.Evaluate(-1.0f, 1.0625) == EDecision::Send);
	TestTrue(TEXT("Quiet packet is held again"), Gate.Evaluate(0.01f, 1.125) == EDecision::Hold);
	TestTrue(TEXT("Loud packet opens the gate with the pre-roll"), Gate.Evaluate(0.2f, 1.1875) == EDecision::SendWithPreRoll);
	TestTrue(TEXT("Loud packet while open has no pre-roll"), Gate.Evaluate(0.2f, 1.25) == EDecision::Send);

	// Hold: quiet packets within 0.3 s of the last loud one are sent.
	TestTrue(TEXT("Word end within the hold is sent"), Gate.Evaluate(0.01f, 1.5) == EDecision::Send);
	TestTrue(TEXT("Quiet after the hold is held"), Gate.Evaluate(0.01f, 1.6) == EDecision::Hold);
	TestEqual(TEXT("Held packets counted"), static_cast<int32>(Gate.GetHeld()), 3);

	// A pre-roll from an earlier phrase is not sent.
	TestTrue(TEXT("Stale pre-roll is dropped"), Gate.Evaluate(0.2f, 3.0) == EDecision::Send);

	// Reset closes the gate and forgets the pre-roll, keeps the threshold.
	Gate.Evaluate(0.01f, 4.0);
	Gate.Reset();
	TestEqual(TEXT("Reset keeps the threshold"), Gate.GetThreshold(), 0.05f);
	TestTrue(TEXT("Reset forgets the pre-roll"), Gate.Evaluate(0.2f, 4.05) == EDecision::Send);
	TestTrue(TEXT("Exactly at the threshold opens"), Gate.Evaluate(0.05f, 10.0) == EDecision::Send);
	return true;
}

namespace
{
	/** Runs the mixer clock to NowSeconds and appends what each output emitted. */
	void AdvanceInto(FChannelMixer& Mixer, double NowSeconds, TArray<TArray<float>>& Out)
	{
		Out.SetNum(Mixer.NumOutputs());
		Mixer.Advance(NowSeconds, [&Out](int32 Output, TConstArrayView<float> Samples)
		{
			Out[Output].Append(Samples.GetData(), Samples.Num());
		});
	}

	TArray<float> MakeConstant(int32 NumSamples, float Value)
	{
		TArray<float> Samples;
		Samples.Init(Value, NumSamples);
		return Samples;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceChannelMixerTimingTest, "SandwichSteam.Voice.Pipeline.ChannelMixerTiming",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceChannelMixerTimingTest::RunTest(const FString& Parameters)
{
	// 8 kHz, 10 ms lead = 80 samples. Output 0: full mix, output 1: everyone but player 7.
	FChannelMixer Mixer;
	Mixer.Configure(8000, 0.01);
	const uint64 Outputs[] = { 0, 7 };
	Mixer.SetOutputs(Outputs);
	TestFalse(TEXT("Idle before any audio"), Mixer.IsRunning());

	const TArray<float> Block = MakeConstant(160, 0.25f);
	Mixer.Add(5, Block.GetData(), Block.Num(), 1.0);
	TestTrue(TEXT("Audio starts the clock"), Mixer.IsRunning());

	TArray<TArray<float>> Out;
	AdvanceInto(Mixer, 1.0, Out);
	TestEqual(TEXT("Nothing is due at the start"), Out[0].Num(), 0);

	AdvanceInto(Mixer, 1.01, Out);
	TestEqual(TEXT("10 ms emitted"), Out[0].Num(), 80);
	TestEqual(TEXT("Every output emits the same length"), Out[1].Num(), 80);
	TestEqual(TEXT("The lead is silence"), Out[0][79], 0.0f);

	AdvanceInto(Mixer, 1.04, Out);
	TestEqual(TEXT("40 ms emitted in total"), Out[0].Num(), 320);
	TestEqual(TEXT("Talker starts one lead late"), Out[0][80], 0.25f);
	TestEqual(TEXT("Talker plays its whole block"), Out[0][239], 0.25f);
	TestEqual(TEXT("Gap after the block is silence"), Out[0][240], 0.0f);
	TestEqual(TEXT("Mix minus another player plays the talker"), Out[1][80], 0.25f);

	// Player 7 talks: the full mix gets them, the mix minus 7 does not.
	Mixer.Add(7, Block.GetData(), Block.Num(), 1.05);
	TestEqual(TEXT("Full mix got audio"), Mixer.GetOutputLastAudio(0), 1.05);
	TestEqual(TEXT("Mix minus 7 did not"), Mixer.GetOutputLastAudio(1), 1.0);
	AdvanceInto(Mixer, 1.08, Out);
	TestTrue(TEXT("Full mix plays player 7 (two talkers: with headroom)"), Out[0][320 + 80 + 40] > 0.1f);
	TestEqual(TEXT("Mix minus 7 stays silent"), Out[1][320 + 80 + 40], 0.0f);

	// A kept output keeps its audio when the outputs change; a new one starts empty.
	Mixer.Add(5, Block.GetData(), Block.Num(), 1.08);
	const uint64 NewOutputs[] = { 9, 0 };
	Mixer.SetOutputs(NewOutputs);
	TestEqual(TEXT("Outputs follow the new order"), Mixer.GetOutputExcluded(1), static_cast<uint64>(0));
	TArray<TArray<float>> After;
	AdvanceInto(Mixer, 1.12, After);
	TestTrue(TEXT("Kept output still has the audio"), After[1].ContainsByPredicate([](float Sample) { return Sample > 0.0f; }));
	TestFalse(TEXT("New output starts empty"), After[0].ContainsByPredicate([](float Sample) { return Sample != 0.0f; }));

	// One second without audio: the channel stops; a stall emits at most one ring.
	TArray<TArray<float>> Stall;
	AdvanceInto(Mixer, 3.0, Stall);
	TestEqual(TEXT("A stall emits at most one second"), Stall[0].Num(), 8000);
	TestFalse(TEXT("Idle after a second of silence"), Mixer.IsRunning());
	TArray<TArray<float>> Stopped;
	AdvanceInto(Mixer, 3.5, Stopped);
	TestEqual(TEXT("A stopped channel emits nothing"), Stopped[0].Num(), 0);

	// Audio more than one second ahead of the clock is dropped and counted.
	const TArray<float> Long = MakeConstant(10000, 0.1f);
	Mixer.Add(5, Long.GetData(), Long.Num(), 4.0);
	TestEqual(TEXT("Overflow counted"), static_cast<int32>(Mixer.GetOverflowSamples()), 10000 - (8000 - 80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceChannelMixerLevelTest, "SandwichSteam.Voice.Pipeline.ChannelMixerLevel",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceChannelMixerLevelTest::RunTest(const FString& Parameters)
{
	const uint64 FullMix[] = { 0 };

	// Two talkers at once: -3 dB, ramped over the first block, steady after.
	{
		FChannelMixer Mixer;
		Mixer.Configure(8000, 0.0);
		Mixer.SetOutputs(FullMix);
		const TArray<float> Block = MakeConstant(800, 0.5f);
		Mixer.Add(1, Block.GetData(), Block.Num(), 0.0);
		Mixer.Add(2, Block.GetData(), Block.Num(), 0.0);

		TArray<TArray<float>> Out;
		AdvanceInto(Mixer, 0.05, Out);
		AdvanceInto(Mixer, 0.1, Out);
		const float Expected = 1.0f * FMath::Pow(10.0f, -3.0f / 20.0f);
		TestTrue(TEXT("Ramp starts near unity gain"), Out[0][0] > Expected);
		TestTrue(TEXT("Two talkers get -3 dB"), FMath::IsNearlyEqual(Out[0][500], Expected, 0.001f));
		TestEqual(TEXT("Two active talkers"), Mixer.GetOutputStats(0, 0.1).ActiveTalkers, 2);
	}

	// Too loud: the soft limiter keeps it below full scale, quiet audio passes unchanged.
	{
		FChannelMixer Mixer;
		Mixer.Configure(8000, 0.0);
		Mixer.SetOutputs(FullMix);
		const TArray<float> Loud = MakeConstant(400, 1.5f);
		const TArray<float> Quiet = MakeConstant(400, -0.3f);
		Mixer.Add(1, Loud.GetData(), Loud.Num(), 0.0);
		Mixer.Add(1, Quiet.GetData(), Quiet.Num(), 0.0);

		TArray<TArray<float>> Out;
		AdvanceInto(Mixer, 0.1, Out);
		TestTrue(TEXT("Limited below full scale"), Out[0][0] < 1.0f && Out[0][0] > FChannelMixer::LimiterKnee);
		TestEqual(TEXT("Quiet audio is untouched"), Out[0][400], -0.3f);
		TestEqual(TEXT("Limited samples counted"), static_cast<int32>(Mixer.GetOutputStats(0, 0.1).Limited), 400);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
