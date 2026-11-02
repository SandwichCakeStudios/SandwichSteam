// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamVoicePacket.h"
#include "Containers/StaticArray.h"

namespace SandwichSteam::Voice
{
	namespace
	{
		FORCEINLINE void WriteInt32(uint8* Out, int32 Value)
		{
			const uint32 Bits = static_cast<uint32>(Value);
			Out[0] = static_cast<uint8>(Bits);
			Out[1] = static_cast<uint8>(Bits >> 8);
			Out[2] = static_cast<uint8>(Bits >> 16);
			Out[3] = static_cast<uint8>(Bits >> 24);
		}

		FORCEINLINE int32 ReadInt32(const uint8* In)
		{
			return static_cast<int32>(static_cast<uint32>(In[0]) | (static_cast<uint32>(In[1]) << 8) | (static_cast<uint32>(In[2]) << 16) | (static_cast<uint32>(In[3]) << 24));
		}

		FORCEINLINE void WriteUInt64(uint8* Out, uint64 Value)
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Out[Index] = static_cast<uint8>(Value >> (8 * Index));
			}
		}

		FORCEINLINE uint64 ReadUInt64(const uint8* In)
		{
			uint64 Value = 0;
			for (int32 Index = 7; Index >= 0; --Index)
			{
				Value = (Value << 8) | In[Index];
			}
			return Value;
		}
	}

	EPacketReadResult ReadPacket(const uint8* Data, int32 Size, FPacketView& OutPacket)
	{
		if (!Data || Size < PacketHeaderBytes)
		{
			return EPacketReadResult::TooSmall;
		}

		OutPacket.Sequence = static_cast<uint16>(Data[0] | (Data[1] << 8));
		OutPacket.Flags = Data[2];
		OutPacket.Talker = 0;
		const int32 Count = Data[3];
		const bool bRelayed = (OutPacket.Flags & PacketFlags::Relayed) != 0;
		if (Size > (bRelayed ? MaxRelayedPacketBytes : MaxPacketBytes))
		{
			return EPacketReadResult::TooLarge;
		}

		if ((OutPacket.Flags & PacketFlags::Hello) != 0)
		{
			if (Size != HelloPacketBytes || Count != 0)
			{
				return EPacketReadResult::MalformedHello;
			}
			OutPacket.NumChannels = 0;
			OutPacket.Payload = nullptr;
			OutPacket.PayloadSize = 0;
			return EPacketReadResult::Hello;
		}

		if (Count > MaxPacketChannels)
		{
			return EPacketReadResult::TooManyChannels;
		}

		const int32 ChannelOffset = PacketHeaderBytes + (bRelayed ? RelayTalkerBytes : 0);
		const int32 PayloadOffset = ChannelOffset + 4 * Count;
		if (PayloadOffset >= Size)
		{
			return EPacketReadResult::NoPayload;
		}

		if (bRelayed)
		{
			OutPacket.Talker = ReadUInt64(Data + PacketHeaderBytes);
			if (OutPacket.Talker == 0)
			{
				return EPacketReadResult::InvalidTalker;
			}
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Channel = ReadInt32(Data + ChannelOffset + 4 * Index);
			if (Channel < 0)
			{
				return EPacketReadResult::InvalidChannelId;
			}
			OutPacket.Channels[Index] = Channel;
		}

		OutPacket.NumChannels = Count;
		OutPacket.Payload = Data + PayloadOffset;
		OutPacket.PayloadSize = Size - PayloadOffset;
		return EPacketReadResult::Voice;
	}

	bool WriteVoicePacket(uint16 Sequence, uint8 Flags, TConstArrayView<int32> Channels, TConstArrayView<uint8> Payload, TArray<uint8>& Out, uint64 Talker)
	{
		Out.Reset();

		const bool bRelayed = Talker != 0;
		const int32 Count = Channels.Num();
		const int32 ChannelOffset = PacketHeaderBytes + (bRelayed ? RelayTalkerBytes : 0);
		const int32 PayloadOffset = ChannelOffset + 4 * Count;
		if (Count > MaxPacketChannels || Payload.IsEmpty() || PayloadOffset + Payload.Num() > (bRelayed ? MaxRelayedPacketBytes : MaxPacketBytes))
		{
			return false;
		}
		for (const int32 Channel : Channels)
		{
			if (Channel < 0)
			{
				return false;
			}
		}

		Out.SetNumUninitialized(PayloadOffset + Payload.Num(), EAllowShrinking::No);
		uint8* Data = Out.GetData();
		Data[0] = static_cast<uint8>(Sequence);
		Data[1] = static_cast<uint8>(Sequence >> 8);
		Data[2] = static_cast<uint8>((Flags & ~(PacketFlags::Hello | PacketFlags::Relayed)) | (bRelayed ? PacketFlags::Relayed : 0));
		Data[3] = static_cast<uint8>(Count);
		if (bRelayed)
		{
			WriteUInt64(Data + PacketHeaderBytes, Talker);
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			WriteInt32(Data + ChannelOffset + 4 * Index, Channels[Index]);
		}
		FMemory::Memcpy(Data + PayloadOffset, Payload.GetData(), Payload.Num());
		return true;
	}

	void WriteHelloPacket(TArray<uint8>& Out)
	{
		Out.Reset();
		Out.SetNumZeroed(HelloPacketBytes, EAllowShrinking::No);
		Out[2] = PacketFlags::Hello;
	}

	uint32 Crc32(const uint8* Data, int32 Size)
	{
		static const TStaticArray<uint32, 256> Table = []()
		{
			TStaticArray<uint32, 256> Result;
			for (uint32 Index = 0; Index < 256; ++Index)
			{
				uint32 Value = Index;
				for (int32 Bit = 0; Bit < 8; ++Bit)
				{
					Value = (Value & 1u) ? (0xEDB88320u ^ (Value >> 1)) : (Value >> 1);
				}
				Result[Index] = Value;
			}
			return Result;
		}();

		uint32 Crc = 0xFFFFFFFFu;
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Crc = Table[(Crc ^ Data[Index]) & 0xFFu] ^ (Crc >> 8);
		}
		return Crc ^ 0xFFFFFFFFu;
	}

	uint64 ReadPayloadSpeaker(TConstArrayView<uint8> Payload)
	{
		if (Payload.Num() < SteamPayloadMinBytes)
		{
			return 0;
		}
		uint64 Speaker = 0;
		for (int32 Index = SteamPayloadIdBytes - 1; Index >= 0; --Index)
		{
			Speaker = (Speaker << 8) | Payload[Index];
		}
		return Speaker;
	}

	bool HasValidPayloadCrc(TConstArrayView<uint8> Payload)
	{
		if (Payload.Num() < SteamPayloadMinBytes)
		{
			return false;
		}
		const int32 CrcOffset = Payload.Num() - SteamPayloadCrcBytes;
		const uint32 Stored = static_cast<uint32>(ReadInt32(Payload.GetData() + CrcOffset));
		return Stored == Crc32(Payload.GetData(), CrcOffset);
	}

	bool RewritePayloadSpeaker(TArrayView<uint8> Payload, uint64 Speaker)
	{
		if (Payload.Num() < SteamPayloadMinBytes)
		{
			return false;
		}
		for (int32 Index = 0; Index < SteamPayloadIdBytes; ++Index)
		{
			Payload[Index] = static_cast<uint8>(Speaker >> (8 * Index));
		}
		const int32 CrcOffset = Payload.Num() - SteamPayloadCrcBytes;
		WriteInt32(Payload.GetData() + CrcOffset, static_cast<int32>(Crc32(Payload.GetData(), CrcOffset)));
		return true;
	}

	const TCHAR* LexPacketReadResult(EPacketReadResult Result)
	{
		switch (Result)
		{
		case EPacketReadResult::Voice:				return TEXT("voice");
		case EPacketReadResult::Hello:				return TEXT("hello");
		case EPacketReadResult::TooSmall:			return TEXT("too small");
		case EPacketReadResult::TooLarge:			return TEXT("too large");
		case EPacketReadResult::TooManyChannels:	return TEXT("too many channels");
		case EPacketReadResult::InvalidChannelId:	return TEXT("negative channel id");
		case EPacketReadResult::NoPayload:			return TEXT("no payload");
		case EPacketReadResult::MalformedHello:		return TEXT("malformed hello");
		case EPacketReadResult::InvalidTalker:		return TEXT("relayed without talker");
		default:									return TEXT("unknown");
		}
	}
}
