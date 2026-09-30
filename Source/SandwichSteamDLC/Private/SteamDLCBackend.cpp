// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamDLCBackend.h"
#include "SteamDLCSubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

FSteamDLCBackend::FSteamDLCBackend(USteamDLCSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	DlcInstalledCallback.Register(this, &FSteamDLCBackend::OnDlcInstalled);
}

void FSteamDLCBackend::ListDLC(TArray<FSteamDLCInfo>& OutDLC) const
{
	ISteamApps* Apps = SteamApps();
	if (!Apps)
	{
		return;
	}

	const int32 Count = Apps->GetDLCCount();
	OutDLC.Reserve(OutDLC.Num() + Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		AppId_t AppId = 0;
		bool bAvailable = false;
		char Name[256] = {};
		if (!Apps->BGetDLCDataByIndex(Index, &AppId, &bAvailable, Name, UE_ARRAY_COUNT(Name)))
		{
			continue;
		}

		FSteamDLCInfo& Info = OutDLC.AddDefaulted_GetRef();
		Info.AppId = static_cast<int32>(AppId);
		Info.Name = UTF8_TO_TCHAR(Name);
		Info.bAvailable = bAvailable;
		Info.bOwned = Apps->BIsSubscribedApp(AppId);
		Info.bInstalled = Apps->BIsDlcInstalled(AppId);
	}
}

bool FSteamDLCBackend::IsOwned(uint32 AppId) const
{
	ISteamApps* Apps = SteamApps();
	return Apps && Apps->BIsSubscribedApp(AppId);
}

bool FSteamDLCBackend::IsInstalled(uint32 AppId) const
{
	ISteamApps* Apps = SteamApps();
	return Apps && Apps->BIsDlcInstalled(AppId);
}

void FSteamDLCBackend::Install(uint32 AppId) const
{
	if (ISteamApps* Apps = SteamApps())
	{
		Apps->InstallDLC(AppId);
	}
}

void FSteamDLCBackend::Uninstall(uint32 AppId) const
{
	if (ISteamApps* Apps = SteamApps())
	{
		Apps->UninstallDLC(AppId);
	}
}

FSteamDLCProgress FSteamDLCBackend::GetProgress(uint32 AppId) const
{
	FSteamDLCProgress Progress;
	ISteamApps* Apps = SteamApps();
	uint64 Downloaded = 0;
	uint64 Total = 0;
	if (Apps && Apps->GetDlcDownloadProgress(AppId, &Downloaded, &Total))
	{
		Progress.bDownloading = true;
		Progress.BytesDownloaded = static_cast<int64>(Downloaded);
		Progress.BytesTotal = static_cast<int64>(Total);
	}
	return Progress;
}

// Runs on Steam's callback thread. Copy the payload, dispatch, return.
void FSteamDLCBackend::OnDlcInstalled(DlcInstalled_t* Payload)
{
	const int32 AppId = static_cast<int32>(Payload->m_nAppID);
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [AppId](USteamDLCSubsystem& DLC)
	{
		DLC.HandleDlcInstalled(AppId);
	});
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

FSteamDLCBackend::FSteamDLCBackend(USteamDLCSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

void FSteamDLCBackend::ListDLC(TArray<FSteamDLCInfo>&) const {}
bool FSteamDLCBackend::IsOwned(uint32) const { return false; }
bool FSteamDLCBackend::IsInstalled(uint32) const { return false; }
void FSteamDLCBackend::Install(uint32) const {}
void FSteamDLCBackend::Uninstall(uint32) const {}
FSteamDLCProgress FSteamDLCBackend::GetProgress(uint32) const { return FSteamDLCProgress(); }

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
