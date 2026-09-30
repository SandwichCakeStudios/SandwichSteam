// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamCloudSaveFormat.h"
#include "Misc/Crc.h"

namespace
{
	// All supported platforms are little endian; the header is copied byte for byte.
	template <typename T>
	void AppendValue(TArray<uint8>& Out, T Value)
	{
		const int32 Offset = Out.AddUninitialized(sizeof(T));
		FMemory::Memcpy(Out.GetData() + Offset, &Value, sizeof(T));
	}

	template <typename T>
	T ReadValue(TConstArrayView<uint8> Bytes, int32 Offset)
	{
		T Value;
		FMemory::Memcpy(&Value, Bytes.GetData() + Offset, sizeof(T));
		return Value;
	}
}

void FSteamCloudSaveFormat::Encode(TConstArrayView<uint8> Payload, int64 UnixTime, TArray<uint8>& OutBytes)
{
	OutBytes.Reset(HeaderSize + Payload.Num());
	AppendValue<uint32>(OutBytes, Magic);
	AppendValue<uint32>(OutBytes, CurrentVersion);
	AppendValue<int64>(OutBytes, UnixTime);
	AppendValue<uint32>(OutBytes, FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
	AppendValue<uint32>(OutBytes, static_cast<uint32>(Payload.Num()));
	OutBytes.Append(Payload.GetData(), Payload.Num());
}

ESteamCloudDecode FSteamCloudSaveFormat::Decode(TConstArrayView<uint8> Bytes, FSteamCloudSaveHeader& OutHeader, TArray<uint8>& OutPayload)
{
	if (Bytes.Num() < HeaderSize)
	{
		return ESteamCloudDecode::TooShort;
	}

	if (ReadValue<uint32>(Bytes, 0) != Magic)
	{
		return ESteamCloudDecode::BadMagic;
	}

	FSteamCloudSaveHeader Header;
	Header.Version = ReadValue<uint32>(Bytes, 4);
	if (Header.Version != CurrentVersion)
	{
		return ESteamCloudDecode::UnsupportedVersion;
	}

	Header.UnixTime = ReadValue<int64>(Bytes, 8);
	Header.Crc32 = ReadValue<uint32>(Bytes, 16);
	Header.PayloadSize = ReadValue<uint32>(Bytes, 20);
	if (static_cast<int64>(Header.PayloadSize) != static_cast<int64>(Bytes.Num()) - HeaderSize)
	{
		return ESteamCloudDecode::BadSize;
	}

	const uint8* Payload = Bytes.GetData() + HeaderSize;
	if (FCrc::MemCrc32(Payload, static_cast<int32>(Header.PayloadSize)) != Header.Crc32)
	{
		return ESteamCloudDecode::BadCrc;
	}

	OutHeader = Header;
	OutPayload.Reset(static_cast<int32>(Header.PayloadSize));
	OutPayload.Append(Payload, static_cast<int32>(Header.PayloadSize));
	return ESteamCloudDecode::Ok;
}

ESteamCloudResolution FSteamCloudSaveFormat::Resolve(ESteamCloudConflictPolicy Policy, const FSteamCloudSaveHeader* Local, const FSteamCloudSaveHeader* Cloud)
{
	if (!Local && !Cloud)
	{
		return ESteamCloudResolution::NothingFound;
	}
	if (!Cloud)
	{
		return ESteamCloudResolution::UseLocal;
	}
	if (!Local)
	{
		return ESteamCloudResolution::UseCloud;
	}

	if (Local->Crc32 == Cloud->Crc32 && Local->PayloadSize == Cloud->PayloadSize && Local->UnixTime == Cloud->UnixTime)
	{
		return ESteamCloudResolution::InSync;
	}

	switch (Policy)
	{
	case ESteamCloudConflictPolicy::PreferLocal:
		return ESteamCloudResolution::UseLocal;
	case ESteamCloudConflictPolicy::PreferCloud:
		return ESteamCloudResolution::UseCloud;
	case ESteamCloudConflictPolicy::Ask:
		return ESteamCloudResolution::Ask;
	case ESteamCloudConflictPolicy::NewestWins:
	default:
		return Cloud->UnixTime > Local->UnixTime ? ESteamCloudResolution::UseCloud : ESteamCloudResolution::UseLocal;
	}
}

const TCHAR* FSteamCloudSaveFormat::DescribeDecode(ESteamCloudDecode Result)
{
	switch (Result)
	{
	case ESteamCloudDecode::TooShort:
		return TEXT("the file is shorter than the header");
	case ESteamCloudDecode::BadMagic:
		return TEXT("the file is not a save slot of this plugin");
	case ESteamCloudDecode::UnsupportedVersion:
		return TEXT("the file was written by a newer version");
	case ESteamCloudDecode::BadSize:
		return TEXT("the file size does not match its header");
	case ESteamCloudDecode::BadCrc:
		return TEXT("the data is corrupt (checksum mismatch)");
	default:
		return TEXT("ok");
	}
}

bool FSteamCloudSaveFormat::IsValidSlotName(const FString& Slot)
{
	if (Slot.IsEmpty() || Slot.Len() > SandwichSteam::Cloud::MaxSlotNameLength)
	{
		return false;
	}

	for (const TCHAR Char : Slot)
	{
		const bool bAllowed = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('A') && Char <= TEXT('Z')) || (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('_') || Char == TEXT('-');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}
