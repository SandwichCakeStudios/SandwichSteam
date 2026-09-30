// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamCloudTypes.h"

/** Header of a slot file. */
struct FSteamCloudSaveHeader
{
	uint32 Version = 0;
	int64 UnixTime = 0;
	uint32 Crc32 = 0;
	uint32 PayloadSize = 0;
};

enum class ESteamCloudDecode : uint8
{
	Ok,
	TooShort,
	BadMagic,
	UnsupportedVersion,
	BadSize,
	BadCrc
};

/**
 * File format of a save slot (local and cloud copy are the same bytes): 24 byte little endian header
 * { Magic 'OSTC', Version, UnixTime, Crc32 of the payload, PayloadSize } followed by the payload.
 * Pure (no SDK, no engine state), so the format and the conflict decisions are unit tested.
 */
class FSteamCloudSaveFormat
{
public:
	static constexpr uint32 Magic = 0x4354534Fu;
	static constexpr uint32 CurrentVersion = 1;
	static constexpr int32 HeaderSize = 24;

	/** Header + payload. */
	static void Encode(TConstArrayView<uint8> Payload, int64 UnixTime, TArray<uint8>& OutBytes);

	/** Checks magic, version, size and CRC. OutPayload and OutHeader are only filled on Ok. */
	static ESteamCloudDecode Decode(TConstArrayView<uint8> Bytes, FSteamCloudSaveHeader& OutHeader, TArray<uint8>& OutPayload);

	/**
	 * Decides between the local and the cloud copy. A null header means the copy does not exist or is corrupt.
	 * Only one copy: it wins. Identical copies: in sync. Otherwise the policy decides; Ask is returned as Ask.
	 */
	static ESteamCloudResolution Resolve(ESteamCloudConflictPolicy Policy, const FSteamCloudSaveHeader* Local, const FSteamCloudSaveHeader* Cloud);

	static const TCHAR* DescribeDecode(ESteamCloudDecode Result);

	/** True for names that are safe as a file name in the cloud and on disk: letters, digits, '_' and '-', at most 100 characters. */
	static bool IsValidSlotName(const FString& Slot);
};
