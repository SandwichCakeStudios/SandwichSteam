// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * The voice wire format (pure, no engine or Steam access, so it can be unit and fuzz tested).
 *
 *   uint16 Sequence | uint8 Flags | uint8 ChannelCount | [uint64 Talker] | int32 ChannelIds[ChannelCount] | Payload (Steam compressed voice)
 *
 * Little endian. A hello is exactly 4 bytes (Hello flag, no channels, no payload) and only opens the Steam session.
 * The sender is never part of a talker's packet: it is the transport's peer identity. Only a packet the relay host forwards carries
 * the talker (Relayed flag), stamped by the host; receivers accept that from the relay host only.
 */
namespace SandwichSteam::Voice
{
	namespace PacketFlags
	{
		/** Play the talker on their own pawn (mouth / proximity speaker). */
		constexpr uint8 Mouth = 1 << 0;
		/** Session hello: no audio. */
		constexpr uint8 Hello = 1 << 1;
		/** Forwarded by the relay host: the talker's Steam ID follows the header. */
		constexpr uint8 Relayed = 1 << 2;
	}

	/** Largest packet a talker sends or a relay host accepts (fits one unfragmented Steam datagram). */
	constexpr int32 MaxPacketBytes = 1200;
	/** The talker id of a relayed packet. */
	constexpr int32 RelayTalkerBytes = 8;
	/** Largest relayed packet: a talker's packet plus the talker id. */
	constexpr int32 MaxRelayedPacketBytes = MaxPacketBytes + RelayTalkerBytes;
	constexpr int32 MaxPacketChannels = 8;
	constexpr int32 PacketHeaderBytes = 4;
	constexpr int32 HelloPacketBytes = 4;

	enum class EPacketReadResult : uint8
	{
		Voice,
		Hello,
		TooSmall,
		TooLarge,
		TooManyChannels,
		InvalidChannelId,
		NoPayload,
		MalformedHello,
		/** Relayed flag with a talker id of 0. */
		InvalidTalker
	};

	/** A parsed packet. Payload points into the buffer that was read; it is only valid while that buffer lives. */
	struct FPacketView
	{
		uint16 Sequence = 0;
		uint8 Flags = 0;
		/** Relayed packets only: the talker the relay host stamped. 0 otherwise. */
		uint64 Talker = 0;
		int32 NumChannels = 0;
		int32 Channels[MaxPacketChannels] = {};
		const uint8* Payload = nullptr;
		int32 PayloadSize = 0;

		bool HasMouth() const { return (Flags & PacketFlags::Mouth) != 0; }
		bool IsRelayed() const { return (Flags & PacketFlags::Relayed) != 0; }
		TConstArrayView<int32> GetChannels() const { return TConstArrayView<int32>(Channels, NumChannels); }
	};

	/** Validates and parses a received packet. Never reads outside [Data, Data + Size). */
	EPacketReadResult ReadPacket(const uint8* Data, int32 Size, FPacketView& OutPacket);

	/**
	 * Writes a voice packet into Out (reset first, its allocation is reused). Talker other than 0 writes a relayed packet (relay host only):
	 * the Relayed flag comes from Talker, never from Flags.
	 * Returns false (Out empty) when the packet would be invalid: too many channels, a negative channel id, no payload, or larger than
	 * MaxPacketBytes (MaxRelayedPacketBytes when relayed).
	 */
	bool WriteVoicePacket(uint16 Sequence, uint8 Flags, TConstArrayView<int32> Channels, TConstArrayView<uint8> Payload, TArray<uint8>& Out, uint64 Talker = 0);

	/** Writes the 4-byte hello into Out. */
	void WriteHelloPacket(TArray<uint8>& Out);

	/** A - B with 16-bit wrap around: positive when A is newer than B (within half the sequence space). */
	FORCEINLINE int32 SequenceDelta(uint16 A, uint16 B)
	{
		return static_cast<int32>(static_cast<int16>(static_cast<uint16>(A - B)));
	}

	/** Short name for logs and the dump. */
	const TCHAR* LexPacketReadResult(EPacketReadResult Result);

	/**
	 * Steam's compressed voice (the payload, what GetVoice returns) as far as it is known, not documented by Valve:
	 *   uint64 SpeakerSteamId | codec sections | uint32 CRC32 of everything before it
	 * DecompressVoice keeps decoder state per speaker id, so the id in the payload decides whose decoder runs.
	 */
	constexpr int32 SteamPayloadIdBytes = 8;
	constexpr int32 SteamPayloadCrcBytes = 4;
	constexpr int32 SteamPayloadMinBytes = SteamPayloadIdBytes + SteamPayloadCrcBytes + 1;

	/** Standard CRC-32 (zlib / IEEE, reflected 0xEDB88320). */
	uint32 Crc32(const uint8* Data, int32 Size);

	/** The speaker Steam ID at the start of a Steam voice payload, 0 when it is too short. */
	uint64 ReadPayloadSpeaker(TConstArrayView<uint8> Payload);

	/** True when the last 4 bytes are the CRC-32 of the rest. */
	bool HasValidPayloadCrc(TConstArrayView<uint8> Payload);

	/** Replaces the speaker Steam ID and recomputes the CRC. False when the payload is too short. */
	bool RewritePayloadSpeaker(TArrayView<uint8> Payload, uint64 Speaker);
}
