// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamUserStatsBackend.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

#include "Core/SteamCallResult.h"
#include "Core/SteamSDK.h"

/** Everything that needs the Steamworks SDK: the callback registrations and the pending call results. */
struct FSteamUserStatsBackend::FImpl
{
	FImpl(FSteamUserStatsBackend& InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
		: Dispatcher(InDispatcher)
		, Owner(&InOwner)
	{
	}

	void Register()
	{
		WeakOwner = Owner->AsShared();
		StatsReceivedCallback.Register(this, &FImpl::OnStatsReceivedRaw);
		StatsStoredCallback.Register(this, &FImpl::OnStatsStoredRaw);
		AchievementStoredCallback.Register(this, &FImpl::OnAchievementStoredRaw);
		AchievementIconCallback.Register(this, &FImpl::OnAchievementIconRaw);
	}

	// The four handlers run on Steam's callback thread. Copy the payload, dispatch, return.

	void OnStatsReceivedRaw(UserStatsReceived_t* Payload)
	{
		const uint64 GameId = Payload->m_nGameID;
		const uint64 UserId = Payload->m_steamIDUser.ConvertToUint64();
		const int32 NativeResult = static_cast<int32>(Payload->m_eResult);
		Dispatcher->Enqueue([Weak = WeakOwner, GameId, UserId, NativeResult]()
		{
			if (const TSharedPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe> Backend = Weak.Pin())
			{
				// Stats of other users (RequestUserStats by someone else) use the same callback.
				if (GameId == Backend->GetAppId() && UserId == Backend->GetLocalSteamId())
				{
					Backend->OnStatsReceived.Broadcast(NativeResult == k_EResultOK, NativeResult);
				}
			}
		});
	}

	void OnStatsStoredRaw(UserStatsStored_t* Payload)
	{
		const uint64 GameId = Payload->m_nGameID;
		const int32 NativeResult = static_cast<int32>(Payload->m_eResult);
		Dispatcher->Enqueue([Weak = WeakOwner, GameId, NativeResult]()
		{
			if (const TSharedPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe> Backend = Weak.Pin())
			{
				if (GameId == Backend->GetAppId())
				{
					Backend->OnStatsStored.Broadcast(NativeResult == k_EResultOK, NativeResult);
				}
			}
		});
	}

	void OnAchievementStoredRaw(UserAchievementStored_t* Payload)
	{
		const uint64 GameId = Payload->m_nGameID;
		const FString ApiName = UTF8_TO_TCHAR(Payload->m_rgchAchievementName);
		const int32 Current = static_cast<int32>(Payload->m_nCurProgress);
		const int32 Max = static_cast<int32>(Payload->m_nMaxProgress);
		Dispatcher->Enqueue([Weak = WeakOwner, GameId, ApiName, Current, Max]()
		{
			if (const TSharedPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe> Backend = Weak.Pin())
			{
				if (GameId == Backend->GetAppId())
				{
					Backend->OnAchievementStored.Broadcast(FName(*ApiName), Current, Max);
				}
			}
		});
	}

	void OnAchievementIconRaw(UserAchievementIconFetched_t* Payload)
	{
		const uint64 GameId = Payload->m_nGameID.ToUint64();
		const FString ApiName = UTF8_TO_TCHAR(Payload->m_rgchAchievementName);
		const int32 Handle = Payload->m_nIconHandle;
		Dispatcher->Enqueue([Weak = WeakOwner, GameId, ApiName, Handle]()
		{
			if (const TSharedPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe> Backend = Weak.Pin())
			{
				if (GameId == Backend->GetAppId())
				{
					Backend->OnAchievementIcon.Broadcast(FName(*ApiName), Handle);
				}
			}
		});
	}

	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
	FSteamUserStatsBackend* Owner;
	TWeakPtr<FSteamUserStatsBackend, ESPMode::ThreadSafe> WeakOwner;

	CCallbackManual<FImpl, UserStatsReceived_t> StatsReceivedCallback;
	CCallbackManual<FImpl, UserStatsStored_t> StatsStoredCallback;
	CCallbackManual<FImpl, UserAchievementStored_t> AchievementStoredCallback;
	CCallbackManual<FImpl, UserAchievementIconFetched_t> AchievementIconCallback;

	TSteamCallResult<GlobalAchievementPercentagesReady_t>::FPendingList PendingPercentages;
};

namespace
{
	FTCHARToUTF8 ToUtf8(FName Name)
	{
		return FTCHARToUTF8(*Name.ToString());
	}
}

FSteamUserStatsBackend::FPtr FSteamUserStatsBackend::Create(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
{
	FPtr Backend = MakeShared<FSteamUserStatsBackend, ESPMode::ThreadSafe>(InDispatcher);
	Backend->BindCallbacks();
	return Backend;
}

FSteamUserStatsBackend::FSteamUserStatsBackend(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Dispatcher(InDispatcher)
	, Impl(MakeUnique<FImpl>(*this, InDispatcher))
{
}

FSteamUserStatsBackend::~FSteamUserStatsBackend() = default;

void FSteamUserStatsBackend::BindCallbacks()
{
	Impl->Register();
}

bool FSteamUserStatsBackend::RequestCurrentStats() const
{
#if SANDWICHSTEAM_WITH_STATS_REQUEST
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->RequestCurrentStats();
#else
	return false;
#endif
}

bool FSteamUserStatsBackend::GetInt(FName ApiName, int32& OutValue) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->GetStat(ToUtf8(ApiName).Get(), &OutValue);
}

bool FSteamUserStatsBackend::GetFloat(FName ApiName, float& OutValue) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->GetStat(ToUtf8(ApiName).Get(), &OutValue);
}

bool FSteamUserStatsBackend::SetInt(FName ApiName, int32 Value) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->SetStat(ToUtf8(ApiName).Get(), Value);
}

bool FSteamUserStatsBackend::SetFloat(FName ApiName, float Value) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->SetStat(ToUtf8(ApiName).Get(), Value);
}

bool FSteamUserStatsBackend::UpdateAvgRate(FName ApiName, float CountThisSession, double SessionSeconds) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->UpdateAvgRateStat(ToUtf8(ApiName).Get(), CountThisSession, SessionSeconds);
}

bool FSteamUserStatsBackend::StoreStats() const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->StoreStats();
}

bool FSteamUserStatsBackend::ResetAllStats(bool bAchievementsToo) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->ResetAllStats(bAchievementsToo);
}

bool FSteamUserStatsBackend::GetAchievement(FName ApiName, bool& bOutUnlocked, int64& OutUnlockUnixTime) const
{
	ISteamUserStats* Stats = SteamUserStats();
	uint32 UnlockTime = 0;
	bool bUnlocked = false;
	if (!Stats || !Stats->GetAchievementAndUnlockTime(ToUtf8(ApiName).Get(), &bUnlocked, &UnlockTime))
	{
		return false;
	}

	bOutUnlocked = bUnlocked;
	OutUnlockUnixTime = bUnlocked ? static_cast<int64>(UnlockTime) : 0;
	return true;
}

bool FSteamUserStatsBackend::SetAchievement(FName ApiName) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->SetAchievement(ToUtf8(ApiName).Get());
}

bool FSteamUserStatsBackend::ClearAchievement(FName ApiName) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->ClearAchievement(ToUtf8(ApiName).Get());
}

bool FSteamUserStatsBackend::IndicateProgress(FName ApiName, uint32 Current, uint32 Max) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->IndicateAchievementProgress(ToUtf8(ApiName).Get(), Current, Max);
}

int32 FSteamUserStatsBackend::GetAchievementIconHandle(FName ApiName) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats ? Stats->GetAchievementIcon(ToUtf8(ApiName).Get()) : 0;
}

FString FSteamUserStatsBackend::GetAchievementAttribute(FName ApiName, const TCHAR* Key) const
{
	ISteamUserStats* Stats = SteamUserStats();
	const char* Value = Stats ? Stats->GetAchievementDisplayAttribute(ToUtf8(ApiName).Get(), TCHAR_TO_UTF8(Key)) : nullptr;
	return Value ? FString(UTF8_TO_TCHAR(Value)) : FString();
}

int32 FSteamUserStatsBackend::GetNumAchievements() const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats ? static_cast<int32>(Stats->GetNumAchievements()) : 0;
}

FName FSteamUserStatsBackend::GetAchievementApiName(int32 Index) const
{
	ISteamUserStats* Stats = SteamUserStats();
	const char* Name = (Stats && Index >= 0) ? Stats->GetAchievementName(static_cast<uint32>(Index)) : nullptr;
	return Name ? FName(UTF8_TO_TCHAR(Name)) : NAME_None;
}

bool FSteamUserStatsBackend::RequestGlobalPercentages(TFunction<void(bool)> OnDone) const
{
	ISteamUserStats* Stats = SteamUserStats();
	if (!Stats)
	{
		return false;
	}

	return TSteamCallResult<GlobalAchievementPercentagesReady_t>::Start(Impl->PendingPercentages, Dispatcher, Stats->RequestGlobalAchievementPercentages(),
		[OnDone = MoveTemp(OnDone)](const GlobalAchievementPercentagesReady_t& Result, bool bIOFailure)
		{
			OnDone(!bIOFailure && Result.m_eResult == k_EResultOK);
		}) != nullptr;
}

bool FSteamUserStatsBackend::GetAchievedPercent(FName ApiName, float& OutPercent) const
{
	ISteamUserStats* Stats = SteamUserStats();
	return Stats && Stats->GetAchievementAchievedPercent(ToUtf8(ApiName).Get(), &OutPercent);
}

uint32 FSteamUserStatsBackend::GetAppId() const
{
	ISteamUtils* Utils = SteamUtils();
	return Utils ? Utils->GetAppID() : 0;
}

uint64 FSteamUserStatsBackend::GetLocalSteamId() const
{
	ISteamUser* User = SteamUser();
	return User ? User->GetSteamID().ConvertToUint64() : 0;
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

struct FSteamUserStatsBackend::FImpl {};

FSteamUserStatsBackend::FPtr FSteamUserStatsBackend::Create(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
{
	return MakeShared<FSteamUserStatsBackend, ESPMode::ThreadSafe>(InDispatcher);
}

FSteamUserStatsBackend::FSteamUserStatsBackend(const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Dispatcher(InDispatcher)
{
}

FSteamUserStatsBackend::~FSteamUserStatsBackend() = default;
void FSteamUserStatsBackend::BindCallbacks() {}
bool FSteamUserStatsBackend::RequestCurrentStats() const { return false; }
bool FSteamUserStatsBackend::GetInt(FName, int32&) const { return false; }
bool FSteamUserStatsBackend::GetFloat(FName, float&) const { return false; }
bool FSteamUserStatsBackend::SetInt(FName, int32) const { return false; }
bool FSteamUserStatsBackend::SetFloat(FName, float) const { return false; }
bool FSteamUserStatsBackend::UpdateAvgRate(FName, float, double) const { return false; }
bool FSteamUserStatsBackend::StoreStats() const { return false; }
bool FSteamUserStatsBackend::ResetAllStats(bool) const { return false; }
bool FSteamUserStatsBackend::GetAchievement(FName, bool&, int64&) const { return false; }
bool FSteamUserStatsBackend::SetAchievement(FName) const { return false; }
bool FSteamUserStatsBackend::ClearAchievement(FName) const { return false; }
bool FSteamUserStatsBackend::IndicateProgress(FName, uint32, uint32) const { return false; }
int32 FSteamUserStatsBackend::GetAchievementIconHandle(FName) const { return 0; }
FString FSteamUserStatsBackend::GetAchievementAttribute(FName, const TCHAR*) const { return FString(); }
int32 FSteamUserStatsBackend::GetNumAchievements() const { return 0; }
FName FSteamUserStatsBackend::GetAchievementApiName(int32) const { return NAME_None; }
bool FSteamUserStatsBackend::RequestGlobalPercentages(TFunction<void(bool)>) const { return false; }
bool FSteamUserStatsBackend::GetAchievedPercent(FName, float&) const { return false; }
uint32 FSteamUserStatsBackend::GetAppId() const { return 0; }
uint64 FSteamUserStatsBackend::GetLocalSteamId() const { return 0; }

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
