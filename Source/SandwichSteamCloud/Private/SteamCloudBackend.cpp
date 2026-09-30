// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamCloudBackend.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

#include "Core/SteamSDK.h"

bool FSteamCloudBackend::IsCloudEnabledForAccount() const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	return Storage && Storage->IsCloudEnabledForAccount();
}

bool FSteamCloudBackend::IsCloudEnabledForApp() const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	return Storage && Storage->IsCloudEnabledForApp();
}

bool FSteamCloudBackend::IsCloudEnabled() const
{
	return IsCloudEnabledForAccount() && IsCloudEnabledForApp();
}

bool FSteamCloudBackend::FileExists(const FString& FileName) const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	return Storage && Storage->FileExists(TCHAR_TO_UTF8(*FileName));
}

bool FSteamCloudBackend::ReadBytes(const FString& FileName, TArray<uint8>& OutBytes) const
{
	OutBytes.Reset();
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	if (!Storage)
	{
		return false;
	}

	const FTCHARToUTF8 Name(*FileName);
	if (!Storage->FileExists(Name.Get()))
	{
		return false;
	}

	const int32 Size = Storage->GetFileSize(Name.Get());
	if (Size < 0)
	{
		return false;
	}
	if (Size == 0)
	{
		return true;
	}

	OutBytes.SetNumUninitialized(Size);
	const int32 Read = Storage->FileRead(Name.Get(), OutBytes.GetData(), Size);
	if (Read != Size)
	{
		OutBytes.Reset();
		return false;
	}
	return true;
}

bool FSteamCloudBackend::WriteBytes(const FString& FileName, TConstArrayView<uint8> Bytes) const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	return Storage && Storage->FileWrite(TCHAR_TO_UTF8(*FileName), Bytes.GetData(), Bytes.Num());
}

bool FSteamCloudBackend::RemoveFile(const FString& FileName) const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	return Storage && Storage->FileDelete(TCHAR_TO_UTF8(*FileName));
}

void FSteamCloudBackend::ListFiles(TArray<FSteamCloudSlotInfo>& OutFiles) const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	if (!Storage)
	{
		return;
	}

	const int32 Count = Storage->GetFileCount();
	OutFiles.Reserve(OutFiles.Num() + Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		int32 Size = 0;
		const char* Name = Storage->GetFileNameAndSize(Index, &Size);
		if (!Name)
		{
			continue;
		}

		FSteamCloudSlotInfo& Info = OutFiles.AddDefaulted_GetRef();
		Info.Slot = UTF8_TO_TCHAR(Name);
		Info.FileSize = Size;
		Info.FileTime = Storage->GetFileTimestamp(Name);
	}
}

bool FSteamCloudBackend::GetQuota(FSteamCloudQuota& OutQuota) const
{
	ISteamRemoteStorage* Storage = SteamRemoteStorage();
	uint64 Total = 0;
	uint64 Available = 0;
	if (!Storage || !Storage->GetQuota(&Total, &Available))
	{
		return false;
	}

	OutQuota.TotalBytes = static_cast<int64>(Total);
	OutQuota.AvailableBytes = static_cast<int64>(Available);
	return true;
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

bool FSteamCloudBackend::IsCloudEnabledForAccount() const { return false; }
bool FSteamCloudBackend::IsCloudEnabledForApp() const { return false; }
bool FSteamCloudBackend::IsCloudEnabled() const { return false; }
bool FSteamCloudBackend::FileExists(const FString&) const { return false; }
bool FSteamCloudBackend::ReadBytes(const FString&, TArray<uint8>& OutBytes) const { OutBytes.Reset(); return false; }
bool FSteamCloudBackend::WriteBytes(const FString&, TConstArrayView<uint8>) const { return false; }
bool FSteamCloudBackend::RemoveFile(const FString&) const { return false; }
void FSteamCloudBackend::ListFiles(TArray<FSteamCloudSlotInfo>&) const {}
bool FSteamCloudBackend::GetQuota(FSteamCloudQuota&) const { return false; }

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
