// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamCloudTypes.h"

/**
 * Raw ISteamRemoteStorage access. The header has no Steamworks types. The calls are synchronous: Steam keeps the files
 * in a local cache and uploads them in the background, so reads and writes only touch the disk. Game thread only, only
 * while Steam is Ready. File names are the slot name plus SandwichSteam::Cloud::SlotExtension.
 */
class FSteamCloudBackend
{
public:
	/** Steam Cloud is on for the account and for this game. Off is a user choice, not an error. */
	bool IsCloudEnabled() const;
	bool IsCloudEnabledForAccount() const;
	bool IsCloudEnabledForApp() const;

	bool FileExists(const FString& FileName) const;

	/** Reads the whole file. False when it does not exist or could not be read. */
	bool ReadBytes(const FString& FileName, TArray<uint8>& OutBytes) const;

	/** Writes the whole file. False when Steam refused (no space, too big, too many files). */
	bool WriteBytes(const FString& FileName, TConstArrayView<uint8> Bytes) const;

	bool RemoveFile(const FString& FileName) const;

	/** Every file of the game in the cloud (all of them, not only slots). */
	void ListFiles(TArray<FSteamCloudSlotInfo>& OutFiles) const;

	bool GetQuota(FSteamCloudQuota& OutQuota) const;
};
