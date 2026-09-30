// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamStatsSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Data/SteamAppDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "SteamStatsFlushPolicy.h"
#include "SteamStatsSettings.h"

#if SANDWICHSTEAM_WITH_DEBUG
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#endif

struct USteamStatsSubsystem::FFlushState
{
	FSteamStatsFlushPolicy Policy;
};

namespace
{
	constexpr int32 MaxQueuedOps = 256;
	constexpr int32 EResultInvalidParam = 8; // k_EResultInvalidParam

	double NowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	const TCHAR* LexStatType(ESteamStatType Type)
	{
		switch (Type)
		{
		case ESteamStatType::Int: return TEXT("Int");
		case ESteamStatType::Float: return TEXT("Float");
		case ESteamStatType::AvgRate: return TEXT("AvgRate");
		default: return TEXT("?");
		}
	}
}

USteamStatsSubsystem* USteamStatsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamStatsSubsystem>() : nullptr;
}

FGameplayTag USteamStatsSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Stats;
}

bool USteamStatsSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	Definition = ToolSettings ? ToolSettings->LoadAppDefinition() : nullptr;
	if (!Definition)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam stats: no App Definition assigned in the Sandwich Steam settings. Stats work by API name only."));
	}

	const USteamStatsSettings* Settings = USteamStatsSettings::Get();
	Flush = MakeShared<FFlushState>();
	Flush->Policy.SetInterval(Settings ? Settings->FlushIntervalSeconds : 5.0);

	Backend = FSteamUserStatsBackend::Create(Dispatcher.ToSharedRef());
	Backend->OnStatsReceived.AddUObject(this, &USteamStatsSubsystem::HandleStatsReceived);
	Backend->OnStatsStored.AddUObject(this, &USteamStatsSubsystem::HandleStatsStored);

	PreExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &USteamStatsSubsystem::FlushBlocking);

	bStatsReady = false;
	if (FSteamUserStatsBackend::NeedsStatsRequest())
	{
		if (!Backend->RequestCurrentStats())
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam stats: RequestCurrentStats was refused. Stat writes stay queued."));
		}
	}
	else
	{
		// Newer SDKs load the stats on their own. Announce readiness on the next tick, after the feature became active.
		ReadyTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
		{
			ReadyTickerHandle.Reset();
			MarkStatsReady();
			return false;
		}));
	}

	return true;
#else
	return false;
#endif
}

void USteamStatsSubsystem::ShutdownFeature()
{
	FlushBlocking();

	if (PreExitHandle.IsValid())
	{
		FCoreDelegates::OnPreExit.Remove(PreExitHandle);
		PreExitHandle.Reset();
	}

	if (FlushTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FlushTickerHandle);
		FlushTickerHandle.Reset();
	}

	if (ReadyTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReadyTickerHandle);
		ReadyTickerHandle.Reset();
	}

	if (Backend.IsValid())
	{
		Backend->OnStatsReceived.RemoveAll(this);
		Backend->OnStatsStored.RemoveAll(this);
	}

	QueuedOps.Reset();
	Flush.Reset();
	Backend.Reset();
	Definition = nullptr;
	bStatsReady = false;
	bReadyBroadcast = false;
	bQueueOverflowLogged = false;
}

FSteamResult USteamStatsSubsystem::ResolveTag(const FGameplayTag& StatTag, EKind Kind, FName& OutApiName) const
{
	if (!Definition)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "StatsNoDefinition", "No Steam App Definition is assigned in the Sandwich Steam settings, so stats cannot be accessed by tag."));
	}

	const FSteamStatDef* Def = Definition->FindStat(StatTag);
	if (!Def)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "StatsUnknownTag", "The tag '{0}' is not a stat of the Steam App Definition."), FText::FromName(StatTag.GetTagName())));
	}

	const bool bTypeMatches = (Kind == EKind::Int) ? (Def->Type == ESteamStatType::Int) : (Def->Type != ESteamStatType::Int);
	if (!bTypeMatches)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "StatsWrongType", "Stat '{0}' is a {1} stat."), FText::FromName(Def->ApiName), FText::FromString(LexStatType(Def->Type))));
	}

	OutApiName = Def->ApiName;
	return FSteamResult::Success();
}

FSteamResult USteamStatsSubsystem::ReadStat(FName ApiName, EKind Kind, double& OutValue) const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!bStatsReady)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "StatsNotReady", "The stats were not received from Steam yet."));
	}

	bool bRead = false;
	if (Kind == EKind::Int)
	{
		int32 Value = 0;
		bRead = Backend->GetInt(ApiName, Value);
		OutValue = Value;
	}
	else
	{
		float Value = 0.f;
		bRead = Backend->GetFloat(ApiName, Value);
		OutValue = Value;
	}

	if (!bRead)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			FText::Format(NSLOCTEXT("SandwichSteam", "StatsReadFailed", "Steam could not read stat '{0}'. Check the API name and type in Steamworks."), FText::FromName(ApiName)));
	}
	return FSteamResult::Success();
}

FSteamResult USteamStatsSubsystem::GetInt(const FGameplayTag& StatTag, int32& OutValue) const
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Int, ApiName);
	return Resolved.IsSuccess() ? GetInt(ApiName, OutValue) : Resolved;
}

FSteamResult USteamStatsSubsystem::GetInt(FName ApiName, int32& OutValue) const
{
	double Value = 0.0;
	const FSteamResult Result = ReadStat(ApiName, EKind::Int, Value);
	OutValue = Result.IsSuccess() ? static_cast<int32>(Value) : 0;
	return Result;
}

FSteamResult USteamStatsSubsystem::GetFloat(const FGameplayTag& StatTag, float& OutValue) const
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Float, ApiName);
	return Resolved.IsSuccess() ? GetFloat(ApiName, OutValue) : Resolved;
}

FSteamResult USteamStatsSubsystem::GetFloat(FName ApiName, float& OutValue) const
{
	double Value = 0.0;
	const FSteamResult Result = ReadStat(ApiName, EKind::Float, Value);
	OutValue = Result.IsSuccess() ? static_cast<float>(Value) : 0.f;
	return Result;
}

FSteamResult USteamStatsSubsystem::SetInt(const FGameplayTag& StatTag, int32 Value)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Int, ApiName);
	return Resolved.IsSuccess() ? SetInt(ApiName, Value) : Resolved;
}

FSteamResult USteamStatsSubsystem::SetInt(FName ApiName, int32 Value)
{
	return Write(ApiName, EOp::SetInt, Value, 0.0);
}

FSteamResult USteamStatsSubsystem::SetFloat(const FGameplayTag& StatTag, float Value)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Float, ApiName);
	return Resolved.IsSuccess() ? SetFloat(ApiName, Value) : Resolved;
}

FSteamResult USteamStatsSubsystem::SetFloat(FName ApiName, float Value)
{
	return Write(ApiName, EOp::SetFloat, Value, 0.0);
}

FSteamResult USteamStatsSubsystem::AddInt(const FGameplayTag& StatTag, int32 Delta)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Int, ApiName);
	return Resolved.IsSuccess() ? AddInt(ApiName, Delta) : Resolved;
}

FSteamResult USteamStatsSubsystem::AddInt(FName ApiName, int32 Delta)
{
	return Write(ApiName, EOp::AddInt, Delta, 0.0);
}

FSteamResult USteamStatsSubsystem::AddFloat(const FGameplayTag& StatTag, float Delta)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Float, ApiName);
	return Resolved.IsSuccess() ? AddFloat(ApiName, Delta) : Resolved;
}

FSteamResult USteamStatsSubsystem::AddFloat(FName ApiName, float Delta)
{
	return Write(ApiName, EOp::AddFloat, Delta, 0.0);
}

FSteamResult USteamStatsSubsystem::UpdateAvgRate(const FGameplayTag& StatTag, float CountThisSession, float SessionSeconds)
{
	FName ApiName;
	const FSteamResult Resolved = ResolveTag(StatTag, EKind::Float, ApiName);
	return Resolved.IsSuccess() ? UpdateAvgRate(ApiName, CountThisSession, SessionSeconds) : Resolved;
}

FSteamResult USteamStatsSubsystem::UpdateAvgRate(FName ApiName, float CountThisSession, float SessionSeconds)
{
	return Write(ApiName, EOp::AvgRate, CountThisSession, SessionSeconds);
}

FSteamResult USteamStatsSubsystem::Write(FName ApiName, EOp Op, double Value, double Seconds)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (ApiName.IsNone() || !FMath::IsFinite(Value) || !FMath::IsFinite(Seconds))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "StatsBadWrite", "The stat name is empty or the value is not a finite number."));
	}

	FQueuedOp Queued;
	Queued.ApiName = ApiName;
	Queued.Op = Op;
	Queued.Value = Value;
	Queued.Seconds = Seconds;

	if (!bStatsReady)
	{
		if (QueuedOps.Num() >= MaxQueuedOps)
		{
			QueuedOps.RemoveAt(0);
			if (!bQueueOverflowLogged)
			{
				bQueueOverflowLogged = true;
				UE_LOG(LogSandwichSteam, Warning, TEXT("Steam stats: more than %d writes queued before the stats arrived. The oldest are dropped."), MaxQueuedOps);
			}
		}
		QueuedOps.Add(Queued);
		return FSteamResult::Success();
	}

	if (!ApplyOp(Queued))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			FText::Format(NSLOCTEXT("SandwichSteam", "StatsWriteFailed", "Steam rejected the write to stat '{0}'. Check the API name and type in Steamworks."), FText::FromName(ApiName)));
	}
	return FSteamResult::Success();
}

double USteamStatsSubsystem::Clamp(FName ApiName, double Value) const
{
	const FSteamStatDef* Def = Definition ? Definition->FindStat(ApiName) : nullptr;
	return (Def && Def->bClampToRange) ? FMath::Clamp(Value, Def->Min, Def->Max) : Value;
}

bool USteamStatsSubsystem::ApplyOp(const FQueuedOp& Op)
{
	if (!Backend.IsValid())
	{
		return false;
	}

	double NewValue = 0.0;
	bool bChanged = false;

	switch (Op.Op)
	{
	case EOp::SetInt:
	case EOp::AddInt:
		{
			double Target = Op.Value;
			if (Op.Op == EOp::AddInt)
			{
				int32 Current = 0;
				if (!Backend->GetInt(Op.ApiName, Current))
				{
					return false;
				}
				Target += Current;
			}

			const double Limited = FMath::Clamp(Clamp(Op.ApiName, Target), -2147483648.0, 2147483647.0);
			const int32 IntValue = static_cast<int32>(FMath::RoundHalfFromZero(Limited));
			bChanged = Backend->SetInt(Op.ApiName, IntValue);
			NewValue = IntValue;
		}
		break;

	case EOp::SetFloat:
	case EOp::AddFloat:
		{
			double Target = Op.Value;
			if (Op.Op == EOp::AddFloat)
			{
				float Current = 0.f;
				if (!Backend->GetFloat(Op.ApiName, Current))
				{
					return false;
				}
				Target += Current;
			}

			const float FloatValue = static_cast<float>(Clamp(Op.ApiName, Target));
			bChanged = Backend->SetFloat(Op.ApiName, FloatValue);
			NewValue = FloatValue;
		}
		break;

	case EOp::AvgRate:
		{
			bChanged = Backend->UpdateAvgRate(Op.ApiName, static_cast<float>(Op.Value), Op.Seconds);
			float Current = 0.f;
			if (bChanged && Backend->GetFloat(Op.ApiName, Current))
			{
				NewValue = Current;
			}
		}
		break;
	}

	if (!bChanged)
	{
		return false;
	}

	MarkStatsDirty(false);
	OnStatChangedNative.Broadcast(Op.ApiName, NewValue);
	return true;
}

void USteamStatsSubsystem::ApplyQueuedOps()
{
	// Applying can broadcast to listeners that write again, so work on a moved-out copy.
	TArray<FQueuedOp> Work = MoveTemp(QueuedOps);
	QueuedOps.Reset();
	bQueueOverflowLogged = false;

	for (const FQueuedOp& Op : Work)
	{
		if (!ApplyOp(Op))
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam stats: queued write to '%s' was rejected by Steam."), *Op.ApiName.ToString());
		}
	}
}

void USteamStatsSubsystem::HandleStatsReceived(bool bSuccess, int32 NativeResult)
{
	if (!IsFeatureActive() && !Backend.IsValid())
	{
		return;
	}

	if (!bSuccess)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam stats: Steam could not deliver the stats (EResult %d). Queued writes (%d) are dropped."), NativeResult, QueuedOps.Num());
		QueuedOps.Reset();
		return;
	}

	MarkStatsReady();
}

void USteamStatsSubsystem::MarkStatsReady()
{
	if (!Backend.IsValid())
	{
		return;
	}

	bStatsReady = true;
	ApplyQueuedOps();

	if (!bReadyBroadcast)
	{
		bReadyBroadcast = true;
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam stats ready."));
		OnStatsReady.Broadcast();
		OnStatsReadyNative.Broadcast();
	}

	EnsureFlushTicker();
}

void USteamStatsSubsystem::HandleStatsStored(bool bSuccess, int32 NativeResult)
{
	if (!Flush.IsValid() || !Flush->Policy.IsStoreInFlight())
	{
		return; // Not a store of ours (another plugin or the OSS stored stats).
	}

	// InvalidParam means Steam refused some values. Retrying the same data would loop, so reload the stats instead.
	const bool bInvalidParam = NativeResult == EResultInvalidParam;
	Flush->Policy.OnFlushCompleted(bSuccess || bInvalidParam, NowSeconds());

	if (bInvalidParam)
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam stats: Steam rejected some stat values (check Min/Max and the stat configuration in Steamworks). Reloading the stats."));
		if (FSteamUserStatsBackend::NeedsStatsRequest() && Backend.IsValid())
		{
			Backend->RequestCurrentStats();
		}
	}

	if (bSuccess)
	{
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam stats stored."));
		OnStatsStoredNative.Broadcast(FSteamResult::Success());
	}
	else
	{
		OnStatsStoredNative.Broadcast(FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			NSLOCTEXT("SandwichSteam", "StatsStoreFailed", "Steam could not store the stats."), NativeResult));
	}

	EnsureFlushTicker();
}

void USteamStatsSubsystem::MarkStatsDirty(bool bUrgent)
{
	if (!Flush.IsValid())
	{
		return;
	}

	if (bUrgent)
	{
		Flush->Policy.MarkUrgent(NowSeconds());
		TryFlush();
	}
	else
	{
		Flush->Policy.MarkDirty(NowSeconds());
	}

	EnsureFlushTicker();
}

FSteamResult USteamStatsSubsystem::StoreStatsNow()
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!bStatsReady)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "StatsNotReadyStore", "The stats were not received from Steam yet."));
	}

	if (Flush->Policy.IsDirty())
	{
		MarkStatsDirty(true);
	}
	return FSteamResult::Success();
}

bool USteamStatsSubsystem::IsStorePending() const
{
	return Flush.IsValid() && (Flush->Policy.IsDirty() || Flush->Policy.IsStoreInFlight());
}

void USteamStatsSubsystem::TryFlush()
{
	if (bStatsReady && Backend.IsValid() && Flush.IsValid() && Flush->Policy.ShouldFlush(NowSeconds()))
	{
		StartStore();
	}
}

void USteamStatsSubsystem::StartStore()
{
	Flush->Policy.OnFlushStarted();
	if (!Backend->StoreStats())
	{
		UE_LOG(LogSandwichSteam, Verbose, TEXT("Steam stats: StoreStats was refused, retrying after the next interval."));
		Flush->Policy.OnFlushCompleted(false, NowSeconds());
		OnStatsStoredNative.Broadcast(FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "StatsStoreRefused", "Steam refused to store the stats.")));
	}
}

void USteamStatsSubsystem::EnsureFlushTicker()
{
	// The ticker only exists while there is something to upload, so an idle feature costs nothing.
	if (!FlushTickerHandle.IsValid() && IsStorePending())
	{
		FlushTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USteamStatsSubsystem::TickFlush), 0.25f);
	}
}

bool USteamStatsSubsystem::TickFlush(float /*DeltaTime*/)
{
	TryFlush();

	if (!bStatsReady || !IsStorePending())
	{
		FlushTickerHandle.Reset();
		return false;
	}
	return true;
}

void USteamStatsSubsystem::FlushBlocking()
{
	// Steam uploads in the background, so this only starts the store. The result is not awaited.
	if (bStatsReady && Backend.IsValid() && Flush.IsValid() && Flush->Policy.IsDirty() && !Flush->Policy.IsStoreInFlight())
	{
		Flush->Policy.OnFlushStarted();
		Backend->StoreStats();
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamStatsSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Stats: %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return Report;
	}

	Report += FString::Printf(TEXT("  Stats ready: %s (SDK needs RequestCurrentStats: %s), queued writes: %d\n"),
		bStatsReady ? TEXT("yes") : TEXT("no"), FSteamUserStatsBackend::NeedsStatsRequest() ? TEXT("yes") : TEXT("no"), QueuedOps.Num());
	Report += FString::Printf(TEXT("  Upload: dirty %s, in flight %s, interval %.1fs\n"),
		Flush->Policy.IsDirty() ? TEXT("yes") : TEXT("no"), Flush->Policy.IsStoreInFlight() ? TEXT("yes") : TEXT("no"), Flush->Policy.GetInterval());
	Report += FString::Printf(TEXT("  Definition: %s (%d stats)\n"), Definition ? *Definition->GetName() : TEXT("none"), Definition ? Definition->Stats.Num() : 0);

	if (Definition && bStatsReady)
	{
		for (const FSteamStatDef& Def : Definition->Stats)
		{
			double Value = 0.0;
			const FSteamResult Read = ReadStat(Def.ApiName, Def.Type == ESteamStatType::Int ? EKind::Int : EKind::Float, Value);
			Report += FString::Printf(TEXT("  %-32s %-8s %s  [%s]\n"), *Def.ApiName.ToString(), LexStatType(Def.Type),
				Read.IsSuccess() ? *FString::SanitizeFloat(Value) : TEXT("<unreadable>"), *Def.Tag.ToString());
		}
	}
	return Report;
}

FSteamResult USteamStatsSubsystem::DebugResetAll(bool bAchievementsToo)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!bStatsReady || !Backend->ResetAllStats(bAchievementsToo))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "StatsResetFailed", "Steam could not reset the stats (not ready yet?)."));
	}

	MarkStatsDirty(true);
	return FSteamResult::Success();
}

FSteamResult USteamStatsSubsystem::DebugExportSchema(FString& OutFilePath) const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!bStatsReady)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotInitialized, NSLOCTEXT("SandwichSteam", "StatsSchemaNotReady", "The stats were not received from Steam yet."));
	}

	const uint32 AppId = Backend->GetAppId();

	// Steam can enumerate achievements at runtime but has no call that lists stats, so stats come from the definition.
	TArray<TSharedPtr<FJsonValue>> Achievements;
	const int32 AchievementCount = Backend->GetNumAchievements();
	for (int32 Index = 0; Index < AchievementCount; ++Index)
	{
		const FName ApiName = Backend->GetAchievementApiName(Index);
		bool bUnlocked = false;
		int64 UnlockTime = 0;
		Backend->GetAchievement(ApiName, bUnlocked, UnlockTime);

		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("apiName"), ApiName.ToString());
		Row->SetStringField(TEXT("displayName"), Backend->GetAchievementAttribute(ApiName, TEXT("name")));
		Row->SetStringField(TEXT("description"), Backend->GetAchievementAttribute(ApiName, TEXT("desc")));
		Row->SetBoolField(TEXT("hidden"), Backend->GetAchievementAttribute(ApiName, TEXT("hidden")) == TEXT("1"));
		Row->SetBoolField(TEXT("unlocked"), bUnlocked);
		Achievements.Add(MakeShared<FJsonValueObject>(Row));
	}

	TArray<TSharedPtr<FJsonValue>> StatRows;
	if (Definition)
	{
		for (const FSteamStatDef& Def : Definition->Stats)
		{
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("apiName"), Def.ApiName.ToString());
			Row->SetStringField(TEXT("type"), LexStatType(Def.Type));
			StatRows.Add(MakeShared<FJsonValueObject>(Row));
		}
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("appId"), AppId);
	Root->SetArrayField(TEXT("achievements"), Achievements);
	Root->SetArrayField(TEXT("stats"), StatRows);
	Root->SetStringField(TEXT("statsNote"), TEXT("Steam cannot list stats at runtime. Stats come from the Steam App Definition asset."));

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "StatsSchemaJson", "Could not serialize the schema."));
	}

	OutFilePath = USteamToolSettings::Get()->GetDataDirectory() / FString::Printf(TEXT("Schema_%u.json"), AppId);
	if (!FFileHelper::SaveStringToFile(Json, *OutFilePath))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, FText::Format(NSLOCTEXT("SandwichSteam", "StatsSchemaWrite", "Could not write {0}."), FText::FromString(OutFilePath)));
	}
	return FSteamResult::Success();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
