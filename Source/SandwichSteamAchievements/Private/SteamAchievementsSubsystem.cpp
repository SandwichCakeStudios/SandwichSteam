// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamAchievementsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamImage.h"
#include "Core/SteamLog.h"
#include "Data/SteamAppDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "SteamAchievementsSettings.h"
#include "SteamProgressThrottle.h"
#include "SteamStatsSubsystem.h"
#include "SteamUserStatsBackend.h"

USteamAchievementsSubsystem* USteamAchievementsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamAchievementsSubsystem>() : nullptr;
}

USteamAchievementsSubsystem::USteamAchievementsSubsystem() = default;
USteamAchievementsSubsystem::~USteamAchievementsSubsystem() = default;

void USteamAchievementsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// The Stats feature must exist (and be initialized) first. It is null when the Stats feature is disabled in the settings.
	Stats = Collection.InitializeDependency<USteamStatsSubsystem>();
	Super::Initialize(Collection);
}

FGameplayTag USteamAchievementsSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Achievements;
}

bool USteamAchievementsSubsystem::InitializeFeature()
{
	if (!Stats)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam achievements need the Steam.Feature.Stats feature, which is disabled."));
		return false;
	}

	const FSteamUserStatsBackend::FPtr Backend = Stats->IsFeatureActive() ? Stats->GetBackend() : nullptr;
	if (!Backend.IsValid())
	{
		return false;
	}

	WeakBackend = Backend;
	Definition = Stats->GetDefinition();

	const USteamAchievementsSettings* Settings = USteamAchievementsSettings::Get();
	Throttle = MakeShared<FSteamProgressThrottle>(Settings ? Settings->ProgressNotifyStepPercent : 10);

	Backend->OnAchievementStored.AddUObject(this, &USteamAchievementsSubsystem::HandleAchievementStored);
	Backend->OnAchievementIcon.AddUObject(this, &USteamAchievementsSubsystem::HandleAchievementIcon);
	Stats->OnStatsReadyNative.AddUObject(this, &USteamAchievementsSubsystem::HandleStatsReady);

	AchievementsByProgressStat.Reset();
	if (Definition && (!Settings || Settings->bAutoIndicateProgressFromStats))
	{
		for (int32 Index = 0; Index < Definition->Achievements.Num(); ++Index)
		{
			const FName ProgressStat = Definition->Achievements[Index].ProgressStat;
			if (!ProgressStat.IsNone())
			{
				AchievementsByProgressStat.FindOrAdd(ProgressStat).Add(Index);
			}
		}

		if (!AchievementsByProgressStat.IsEmpty())
		{
			Stats->OnStatChangedNative.AddUObject(this, &USteamAchievementsSubsystem::HandleStatChanged);
		}
	}

	return true;
}

void USteamAchievementsSubsystem::ShutdownFeature()
{
	if (Stats)
	{
		Stats->OnStatsReadyNative.RemoveAll(this);
		Stats->OnStatChangedNative.RemoveAll(this);
	}

	if (const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin())
	{
		Backend->OnAchievementStored.RemoveAll(this);
		Backend->OnAchievementIcon.RemoveAll(this);
	}

	// Outstanding requests are cancelled by the async actions (they observe OnFeatureActiveChanged).
	WeakBackend.Reset();
	Throttle.Reset();
	Definition = nullptr;
	AchievementsByProgressStat.Reset();
	QueuedUnlocks.Reset();
	PendingIcons.Reset();
	PendingPercentages.Reset();
	IconCache.Reset();
	GlobalPercents.Reset();
	bPercentagesRequested = false;
}

FSteamResult USteamAchievementsSubsystem::ResolveTag(const FGameplayTag& AchievementTag, FName& OutApiName) const
{
	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "AchNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so achievements cannot be accessed by tag."));
	}

	const FSteamAchievementDef* Def = Definition->FindAchievement(AchievementTag);
	if (!Def)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "AchUnknownTag", "The tag '{0}' is not an achievement of the Steam App Definition."), FText::FromName(AchievementTag.GetTagName())));
	}

	OutApiName = Def->ApiName;
	return FSteamResult::Success();
}

FSteamResult USteamAchievementsSubsystem::RequireStatsReady() const
{
	if (!Stats || !Stats->AreStatsReady())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "AchStatsNotReady", "The stats were not received from Steam yet."));
	}
	return FSteamResult::Success();
}

FSteamResult USteamAchievementsSubsystem::Unlock(const FGameplayTag& AchievementTag)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(AchievementTag, ApiName);
	return Resolved.IsSuccess() ? Unlock(ApiName) : Resolved;
}

FSteamResult USteamAchievementsSubsystem::Unlock(FName ApiName)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (ApiName.IsNone())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "AchEmptyName", "The achievement name is empty."));
	}

	if (!Stats->AreStatsReady())
	{
		QueuedUnlocks.AddUnique(ApiName);
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam achievement '%s' queued until the stats are ready."), *ApiName.ToString());
		return FSteamResult::Success();
	}

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (!Backend.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "AchNoBackend", "The Steam stats backend is not available."));
	}

	bool bUnlocked = false;
	int64 UnlockTime = 0;
	if (!Backend->GetAchievement(ApiName, bUnlocked, UnlockTime))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "AchUnknownName", "Steam does not know the achievement '{0}'. Check the API name in Steamworks."), FText::FromName(ApiName)));
	}

	if (bUnlocked)
	{
		return FSteamResult::Success(); // Already unlocked: no Steam call, no store.
	}

	if (!Backend->SetAchievement(ApiName))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, FText::Format(NSLOCTEXT("SandwichSteam", "AchSetFailed", "Steam refused to unlock '{0}'."), FText::FromName(ApiName)));
	}

	IconCache.Remove(ApiName);
	Throttle->Forget(ApiName);
	Stats->MarkStatsDirty(true);
	return FSteamResult::Success();
}

FSteamResult USteamAchievementsSubsystem::Clear(const FGameplayTag& AchievementTag)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(AchievementTag, ApiName);
	return Resolved.IsSuccess() ? Clear(ApiName) : Resolved;
}

FSteamResult USteamAchievementsSubsystem::Clear(FName ApiName)
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (!Stats->AreStatsReady() || !Backend.IsValid() || !Backend->ClearAchievement(ApiName))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, FText::Format(NSLOCTEXT("SandwichSteam", "AchClearFailed", "Steam could not lock '{0}' again."), FText::FromName(ApiName)));
	}

	IconCache.Remove(ApiName);
	Throttle->Forget(ApiName);
	Stats->MarkStatsDirty(true);
	return FSteamResult::Success();
#else
	return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "AchClearShipping", "Clearing achievements is only available in non-Shipping builds."));
#endif
}

FSteamResult USteamAchievementsSubsystem::GetState(const FGameplayTag& AchievementTag, bool& bOutUnlocked, FDateTime& OutUnlockTime) const
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(AchievementTag, ApiName);
	return Resolved.IsSuccess() ? GetState(ApiName, bOutUnlocked, OutUnlockTime) : Resolved;
}

FSteamResult USteamAchievementsSubsystem::GetState(FName ApiName, bool& bOutUnlocked, FDateTime& OutUnlockTime) const
{
	bOutUnlocked = false;
	OutUnlockTime = FDateTime::FromUnixTimestamp(0);

	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = RequireStatsReady();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	int64 UnlockTime = 0;
	if (!Backend.IsValid() || !Backend->GetAchievement(ApiName, bOutUnlocked, UnlockTime))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "AchStateUnknown", "Steam does not know the achievement '{0}'."), FText::FromName(ApiName)));
	}

	OutUnlockTime = FDateTime::FromUnixTimestamp(UnlockTime);
	return FSteamResult::Success();
}

void USteamAchievementsSubsystem::BuildInfo(FName ApiName, const FSteamAchievementDef* Def, FSteamAchievementInfo& OutInfo) const
{
	OutInfo = FSteamAchievementInfo();
	OutInfo.ApiName = ApiName;
	OutInfo.Tag = Def ? Def->Tag : FGameplayTag();

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (!Backend.IsValid())
	{
		return;
	}

	OutInfo.DisplayName = Backend->GetAchievementAttribute(ApiName, TEXT("name"));
	OutInfo.Description = Backend->GetAchievementAttribute(ApiName, TEXT("desc"));
	OutInfo.bHidden = Backend->GetAchievementAttribute(ApiName, TEXT("hidden")) == TEXT("1");

	int64 UnlockTime = 0;
	Backend->GetAchievement(ApiName, OutInfo.bUnlocked, UnlockTime);
	OutInfo.UnlockTime = FDateTime::FromUnixTimestamp(UnlockTime);

	if (Def && !Def->ProgressStat.IsNone() && Def->ProgressMax > 0)
	{
		OutInfo.ProgressMax = Def->ProgressMax;

		const FSteamStatDef* StatDef = Definition ? Definition->FindStat(Def->ProgressStat) : nullptr;
		if (StatDef && StatDef->Type != ESteamStatType::Int)
		{
			float Value = 0.f;
			if (Stats->GetFloat(Def->ProgressStat, Value).IsSuccess())
			{
				OutInfo.ProgressCurrent = FMath::RoundToInt(Value);
			}
		}
		else
		{
			int32 Value = 0;
			if (Stats->GetInt(Def->ProgressStat, Value).IsSuccess())
			{
				OutInfo.ProgressCurrent = Value;
			}
		}
	}
}

FSteamResult USteamAchievementsSubsystem::GetInfo(const FGameplayTag& AchievementTag, FSteamAchievementInfo& OutInfo) const
{
	OutInfo = FSteamAchievementInfo();

	FName ApiName;
	const FSteamResult Resolved = ResolveTag(AchievementTag, ApiName);
	return Resolved.IsSuccess() ? GetInfo(ApiName, OutInfo) : Resolved;
}

FSteamResult USteamAchievementsSubsystem::GetInfo(FName ApiName, FSteamAchievementInfo& OutInfo) const
{
	OutInfo = FSteamAchievementInfo();

	// GetState checks active, stats ready and that Steam knows the name.
	bool bUnlocked = false;
	FDateTime UnlockTime;
	const FSteamResult Result = GetState(ApiName, bUnlocked, UnlockTime);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	BuildInfo(ApiName, Definition ? Definition->FindAchievement(ApiName) : nullptr, OutInfo);
	return FSteamResult::Success();
}

void USteamAchievementsSubsystem::GetAllInfo(TArray<FSteamAchievementInfo>& OutInfos) const
{
	OutInfos.Reset();

	FSteamResult Result;
	if (!RequireActive(Result) || !RequireStatsReady().IsSuccess())
	{
		return;
	}

	if (Definition && !Definition->Achievements.IsEmpty())
	{
		for (const FSteamAchievementDef& Def : Definition->Achievements)
		{
			BuildInfo(Def.ApiName, &Def, OutInfos.AddDefaulted_GetRef());
		}
		return;
	}

	// No definition: fall back to what Steam reports.
	if (const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin())
	{
		const int32 Count = Backend->GetNumAchievements();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			BuildInfo(Backend->GetAchievementApiName(Index), nullptr, OutInfos.AddDefaulted_GetRef());
		}
	}
}

FSteamResult USteamAchievementsSubsystem::IndicateProgress(const FGameplayTag& AchievementTag, int32 CurrentValue)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	FName ApiName;
	Result = ResolveTag(AchievementTag, ApiName);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	return IndicateProgress(ApiName, CurrentValue);
}

FSteamResult USteamAchievementsSubsystem::IndicateProgress(FName ApiName, int32 CurrentValue, int32 MaxValue)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (ApiName.IsNone())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "AchEmptyNameProgress", "The achievement name is empty."));
	}

	if (MaxValue <= 0)
	{
		const FSteamAchievementDef* Def = Definition ? Definition->FindAchievement(ApiName) : nullptr;
		MaxValue = Def ? Def->ProgressMax : 0;
	}
	return IndicateProgressInternal(ApiName, CurrentValue, MaxValue);
}

FSteamResult USteamAchievementsSubsystem::IndicateProgressInternal(FName ApiName, int32 CurrentValue, int32 MaxValue)
{
	FSteamResult Result = RequireStatsReady();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (MaxValue <= 0)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "AchNoProgressMax", "Achievement '{0}' has no maximum: pass Max Value, or set Progress Max on its row in the Steam App Definition."), FText::FromName(ApiName)));
	}

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (!Backend.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "AchNoBackendProgress", "The Steam stats backend is not available."));
	}

	bool bUnlocked = false;
	int64 UnlockTime = 0;
	if (!Backend->GetAchievement(ApiName, bUnlocked, UnlockTime))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "AchProgressUnknown", "Steam does not know the achievement '{0}'. Check the API name in Steamworks."), FText::FromName(ApiName)));
	}

	if (bUnlocked)
	{
		return FSteamResult::Success(); // Nothing to show for a finished achievement.
	}

	if (!Throttle->ShouldIndicate(ApiName, CurrentValue, MaxValue))
	{
		return FSteamResult::Success(); // Throttled, at maximum, or no progress yet.
	}

	if (!Backend->IndicateProgress(ApiName, static_cast<uint32>(CurrentValue), static_cast<uint32>(MaxValue)))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, FText::Format(NSLOCTEXT("SandwichSteam", "AchProgressFailed", "Steam refused the progress of '{0}'."), FText::FromName(ApiName)));
	}
	return FSteamResult::Success();
}

FSteamResult USteamAchievementsSubsystem::RequestIcon(const FGameplayTag& AchievementTag, FSteamAchievementIconDelegate OnComplete)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(AchievementTag, ApiName);
	return Resolved.IsSuccess() ? RequestIcon(ApiName, MoveTemp(OnComplete)) : Resolved;
}

FSteamResult USteamAchievementsSubsystem::RequestIcon(FName ApiName, FSteamAchievementIconDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = RequireStatsReady();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (const TObjectPtr<UTexture2D>* Cached = IconCache.Find(ApiName))
	{
		OnComplete.ExecuteIfBound(FSteamResult::Success(), Cached->Get());
		return FSteamResult::Success();
	}

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (!Backend.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "AchNoBackendIcon", "The Steam stats backend is not available."));
	}

	const int32 ImageHandle = Backend->GetAchievementIconHandle(ApiName);
	if (ImageHandle > 0)
	{
		DeliverIcon(ApiName, ImageHandle, OnComplete);
	}
	else
	{
		// Steam is fetching it and reports through UserAchievementIconFetched_t.
		FPendingIcon& Pending = PendingIcons.AddDefaulted_GetRef();
		Pending.ApiName = ApiName;
		Pending.Callback = MoveTemp(OnComplete);
	}
	return FSteamResult::Success();
}

void USteamAchievementsSubsystem::DeliverIcon(FName ApiName, int32 ImageHandle, const FSteamAchievementIconDelegate& Callback)
{
	FSteamImagePixels Pixels;
	UTexture2D* Texture = (ImageHandle > 0 && SandwichSteam::ReadSteamImage(ImageHandle, Pixels)) ? SandwichSteam::CreateTextureFromPixels(Pixels) : nullptr;
	if (!Texture)
	{
		Callback.ExecuteIfBound(FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
			FText::Format(NSLOCTEXT("SandwichSteam", "AchNoIcon", "Steam has no icon for the achievement '{0}'."), FText::FromName(ApiName))), nullptr);
		return;
	}

	IconCache.Add(ApiName, Texture);
	Callback.ExecuteIfBound(FSteamResult::Success(), Texture);
}

FSteamResult USteamAchievementsSubsystem::RequestGlobalPercentages(FSteamAchievementPercentagesDelegate OnComplete)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = RequireStatsReady();
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (!Backend.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "AchNoBackendPercent", "The Steam stats backend is not available."));
	}

	PendingPercentages.Add(MoveTemp(OnComplete));
	if (PendingPercentages.Num() > 1)
	{
		return FSteamResult::Success(); // A request is already running, this one shares its answer.
	}

	const bool bStarted = Backend->RequestGlobalPercentages([WeakThis = TWeakObjectPtr<USteamAchievementsSubsystem>(this)](bool bSuccess)
	{
		if (USteamAchievementsSubsystem* Self = WeakThis.Get())
		{
			Self->HandlePercentagesReady(bSuccess);
		}
	});

	if (!bStarted)
	{
		PendingPercentages.Reset();
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "AchPercentRefused", "Steam refused the global achievement percentages request."));
	}
	return FSteamResult::Success();
}

bool USteamAchievementsSubsystem::GetGlobalPercent(const FGameplayTag& AchievementTag, float& OutPercent) const
{
	OutPercent = 0.f;

	FName ApiName;
	return ResolveTag(AchievementTag, ApiName).IsSuccess() && GetGlobalPercent(ApiName, OutPercent);
}

bool USteamAchievementsSubsystem::GetGlobalPercent(FName ApiName, float& OutPercent) const
{
	OutPercent = 0.f;
	if (!bPercentagesRequested)
	{
		return false;
	}

	if (const float* Percent = GlobalPercents.Find(ApiName))
	{
		OutPercent = *Percent;
		return true;
	}
	return false;
}

void USteamAchievementsSubsystem::HandleStatsReady()
{
	if (!IsFeatureActive())
	{
		return;
	}

	TArray<FName> Work = MoveTemp(QueuedUnlocks);
	QueuedUnlocks.Reset();
	for (const FName ApiName : Work)
	{
		const FSteamResult Result = Unlock(ApiName);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam achievement '%s' could not be unlocked after the stats arrived: %s"), *ApiName.ToString(), *Result.Message.ToString());
		}
	}
}

void USteamAchievementsSubsystem::HandleStatChanged(FName StatApiName, double NewValue)
{
	if (!IsFeatureActive() || !Definition)
	{
		return;
	}

	const TArray<int32>* Indices = AchievementsByProgressStat.Find(StatApiName);
	if (!Indices)
	{
		return;
	}

	for (const int32 Index : *Indices)
	{
		if (Definition->Achievements.IsValidIndex(Index))
		{
			const FSteamAchievementDef& Def = Definition->Achievements[Index];
			const FSteamResult Result = IndicateProgressInternal(Def.ApiName, FMath::RoundToInt(NewValue), Def.ProgressMax);
			if (!Result.IsSuccess())
			{
				UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam achievement progress for '%s' skipped: %s"), *Definition->Achievements[Index].ApiName.ToString(), *Result.Message.ToString());
			}
		}
	}
}

void USteamAchievementsSubsystem::HandleAchievementStored(FName ApiName, int32 Current, int32 Max)
{
	if (!IsFeatureActive())
	{
		return;
	}

	const FSteamAchievementDef* Def = Definition ? Definition->FindAchievement(ApiName) : nullptr;
	const FGameplayTag Tag = Def ? Def->Tag : FGameplayTag();

	if (Max == 0)
	{
		// Progress 0/0 means the achievement is unlocked.
		IconCache.Remove(ApiName);
		Throttle->Forget(ApiName);
		OnAchievementUnlocked.Broadcast(Tag, ApiName);
	}
	else
	{
		OnAchievementProgress.Broadcast(Tag, ApiName, Current, Max);
	}
}

void USteamAchievementsSubsystem::HandleAchievementIcon(FName ApiName, int32 ImageHandle)
{
	if (!IsFeatureActive() || PendingIcons.IsEmpty())
	{
		return;
	}

	// Callbacks may request again, so pull the matching requests out first.
	TArray<FPendingIcon> Matching;
	for (int32 Index = PendingIcons.Num() - 1; Index >= 0; --Index)
	{
		if (PendingIcons[Index].ApiName == ApiName)
		{
			Matching.Insert(MoveTemp(PendingIcons[Index]), 0);
			PendingIcons.RemoveAt(Index);
		}
	}

	for (const FPendingIcon& Pending : Matching)
	{
		DeliverIcon(ApiName, ImageHandle, Pending.Callback);
	}
}

void USteamAchievementsSubsystem::HandlePercentagesReady(bool bSuccess)
{
	if (!IsFeatureActive())
	{
		return;
	}

	TArray<FSteamAchievementPercentagesDelegate> Work = MoveTemp(PendingPercentages);
	PendingPercentages.Reset();

	FSteamResult Result = FSteamResult::Success();
	const FSteamUserStatsBackend::FPtr Backend = WeakBackend.Pin();
	if (bSuccess && Backend.IsValid())
	{
		GlobalPercents.Reset();
		const int32 Count = Backend->GetNumAchievements();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FName ApiName = Backend->GetAchievementApiName(Index);
			float Percent = 0.f;
			if (Backend->GetAchievedPercent(ApiName, Percent))
			{
				GlobalPercents.Add(ApiName, Percent);
			}
		}
		bPercentagesRequested = true;
	}
	else
	{
		Result = FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "AchPercentFailed", "Steam could not deliver the global achievement percentages."));
	}

	for (const FSteamAchievementPercentagesDelegate& Callback : Work)
	{
		Callback.ExecuteIfBound(Result);
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamAchievementsSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Achievements: %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive())
	{
		return Report;
	}

	Report += FString::Printf(TEXT("  Stats ready: %s, queued unlocks: %d, cached icons: %d, pending icons: %d\n"),
		(Stats && Stats->AreStatsReady()) ? TEXT("yes") : TEXT("no"), QueuedUnlocks.Num(), IconCache.Num(), PendingIcons.Num());

	TArray<FSteamAchievementInfo> Infos;
	GetAllInfo(Infos);
	int32 UnlockedCount = 0;
	for (const FSteamAchievementInfo& Info : Infos)
	{
		UnlockedCount += Info.bUnlocked ? 1 : 0;
	}
	Report += FString::Printf(TEXT("  Achievements: %d (%d unlocked)\n"), Infos.Num(), UnlockedCount);

	for (const FSteamAchievementInfo& Info : Infos)
	{
		Report += FString::Printf(TEXT("  [%s] %-28s %s%s\n"), Info.bUnlocked ? TEXT("x") : TEXT(" "), *Info.ApiName.ToString(), *Info.DisplayName,
			Info.ProgressMax > 0 ? *FString::Printf(TEXT("  (%d/%d)"), Info.ProgressCurrent, Info.ProgressMax) : TEXT(""));
	}
	return Report;
}
#endif
