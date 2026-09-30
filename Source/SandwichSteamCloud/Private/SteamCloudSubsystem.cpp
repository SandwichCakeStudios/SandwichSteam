// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamCloudSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SteamCloudBackend.h"
#include "SteamCloudSaveFormat.h"
#include "SteamCloudSettings.h"

/** One copy (local or cloud) of a slot as read from storage. */
struct USteamCloudSubsystem::FCopy
{
	/** A file exists (it may still be corrupt). */
	bool bExists = false;
	/** The file passed its integrity check. */
	bool bValid = false;
	ESteamCloudDecode Decode = ESteamCloudDecode::Ok;
	FSteamCloudSaveHeader Header;
	/** The whole file (header + payload), as written. */
	TArray<uint8> Bytes;
	TArray<uint8> Payload;

	FSteamCloudSaveInfo ToInfo() const
	{
		FSteamCloudSaveInfo Info;
		Info.bValid = bValid;
		Info.UnixTime = bValid ? Header.UnixTime : 0;
		Info.PayloadSize = bValid ? static_cast<int32>(Header.PayloadSize) : 0;
		return Info;
	}
};

namespace
{
	FString FileNameOf(const FString& Slot)
	{
		return Slot + SandwichSteam::Cloud::SlotExtension;
	}
}

USteamCloudSubsystem* USteamCloudSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamCloudSubsystem>() : nullptr;
}

USteamCloudSubsystem::USteamCloudSubsystem() = default;
USteamCloudSubsystem::~USteamCloudSubsystem() = default;

FGameplayTag USteamCloudSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Cloud;
}

bool USteamCloudSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	Backend = MakeShared<FSteamCloudBackend>();
	return true;
#else
	return false;
#endif
}

void USteamCloudSubsystem::ShutdownFeature()
{
	Backend.Reset();
}

FString USteamCloudSubsystem::GetLocalPath(const FString& Slot)
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / FileNameOf(Slot);
}

FSteamResult USteamCloudSubsystem::ValidateSlot(const FString& Slot) const
{
	if (!FSteamCloudSaveFormat::IsValidSlotName(Slot))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			NSLOCTEXT("SandwichSteam", "CloudBadSlot", "A slot name may only contain letters, digits, '_' and '-' and has at most 100 characters."));
	}
	return FSteamResult::Success();
}

bool USteamCloudSubsystem::IsCloudEnabled() const
{
	return Backend.IsValid() && Backend->IsCloudEnabled();
}

bool USteamCloudSubsystem::GetQuota(FSteamCloudQuota& OutQuota) const
{
	return Backend.IsValid() && Backend->GetQuota(OutQuota);
}

USteamCloudSubsystem::FCopy USteamCloudSubsystem::ReadLocal(const FString& Slot) const
{
	FCopy Copy;
	if (FFileHelper::LoadFileToArray(Copy.Bytes, *GetLocalPath(Slot)))
	{
		Copy.bExists = true;
		Copy.Decode = FSteamCloudSaveFormat::Decode(Copy.Bytes, Copy.Header, Copy.Payload);
		Copy.bValid = Copy.Decode == ESteamCloudDecode::Ok;
	}
	return Copy;
}

USteamCloudSubsystem::FCopy USteamCloudSubsystem::ReadCloud(const FString& Slot) const
{
	FCopy Copy;
	if (Backend.IsValid() && Backend->IsCloudEnabled() && Backend->ReadBytes(FileNameOf(Slot), Copy.Bytes))
	{
		Copy.bExists = true;
		Copy.Decode = FSteamCloudSaveFormat::Decode(Copy.Bytes, Copy.Header, Copy.Payload);
		Copy.bValid = Copy.Decode == ESteamCloudDecode::Ok;
	}
	return Copy;
}

bool USteamCloudSubsystem::WriteLocal(const FString& Slot, const TArray<uint8>& Bytes) const
{
	// Write next to the file and move it over, so a crash never leaves a half written slot.
	const FString Path = GetLocalPath(Slot);
	const FString TempPath = Path + TEXT(".tmp");
	if (!FFileHelper::SaveArrayToFile(Bytes, *TempPath))
	{
		return false;
	}

	if (!IFileManager::Get().Move(*Path, *TempPath, /*Replace*/ true, /*EvenIfReadOnly*/ true))
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		return false;
	}
	return true;
}

FSteamResult USteamCloudSubsystem::WriteCloud(const FString& Slot, const TArray<uint8>& Bytes, bool& bOutWritten) const
{
	bOutWritten = false;
	if (Backend->WriteBytes(FileNameOf(Slot), Bytes))
	{
		bOutWritten = true;
		return FSteamResult::Success();
	}

	FSteamCloudQuota Quota;
	if (Backend->GetQuota(Quota) && Quota.AvailableBytes < Bytes.Num())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_QuotaExceeded,
			NSLOCTEXT("SandwichSteam", "CloudQuota", "The Steam Cloud space of this game is full. The local file was saved, the cloud copy was not."));
	}

	return FSteamResult::Failure(SteamGameplayTags::Error_Failed,
		NSLOCTEXT("SandwichSteam", "CloudWriteFailed", "Steam refused the cloud write. The local file was saved, the cloud copy was not."));
}

FSteamCloudSaveResult USteamCloudSubsystem::SaveBytes(const FString& Slot, TConstArrayView<uint8> Payload)
{
	FSteamCloudSaveResult Out;
	if (!RequireActive(Out.Result))
	{
		return Out;
	}

	Out.Result = ValidateSlot(Slot);
	if (!Out.Result.IsSuccess())
	{
		return Out;
	}

	TArray<uint8> Bytes;
	FSteamCloudSaveFormat::Encode(Payload, FDateTime::UtcNow().ToUnixTimestamp(), Bytes);

	if (!WriteLocal(Slot, Bytes))
	{
		Out.Result = FSteamResult::Failure(SteamGameplayTags::Error_Failed,
			FText::Format(NSLOCTEXT("SandwichSteam", "CloudLocalWriteFailed", "Could not write the local save file {0}."), FText::FromString(GetLocalPath(Slot))));
		return Out;
	}
	Out.bSavedLocal = true;

	if (!Backend->IsCloudEnabled())
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam cloud: Steam Cloud is turned off for this account or game. Slot '%s' was saved locally only."), *Slot);
		Out.Result = FSteamResult::Success();
		return Out;
	}

	Out.Result = WriteCloud(Slot, Bytes, Out.bSavedCloud);
	if (!Out.Result.IsSuccess())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam cloud: slot '%s': %s"), *Slot, *Out.Result.Message.ToString());
	}
	return Out;
}

FSteamCloudLoadResult USteamCloudSubsystem::Finish(const FString& Slot, ESteamCloudResolution Resolution, const FCopy& Local, const FCopy& Cloud, TArray<uint8>& OutPayload)
{
	FSteamCloudLoadResult Out;
	OutPayload.Reset();

	switch (Resolution)
	{
	case ESteamCloudResolution::NothingFound:
	{
		if (Local.bExists || Cloud.bExists)
		{
			const ESteamCloudDecode Reason = Local.bExists ? Local.Decode : Cloud.Decode;
			Out.Outcome = ESteamCloudLoadOutcome::Corrupt;
			Out.Result = FSteamResult::Failure(SteamGameplayTags::Error_Failed,
				FText::Format(NSLOCTEXT("SandwichSteam", "CloudCorrupt", "The save slot '{0}' is damaged and no good copy exists: {1}."), FText::FromString(Slot), FText::FromString(FSteamCloudSaveFormat::DescribeDecode(Reason))));
		}
		else
		{
			Out.Outcome = ESteamCloudLoadOutcome::NotFound;
			Out.Result = FSteamResult::Success();
		}
		return Out;
	}

	case ESteamCloudResolution::Ask:
		Out.Outcome = ESteamCloudLoadOutcome::Conflict;
		Out.Result = FSteamResult::Success();
		OnCloudConflict.Broadcast(Slot, Local.ToInfo(), Cloud.ToInfo());
		return Out;

	case ESteamCloudResolution::UseCloud:
		if (Local.bExists && !Local.bValid)
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam cloud: the local file of slot '%s' is damaged (%s). Using the cloud copy."), *Slot, FSteamCloudSaveFormat::DescribeDecode(Local.Decode));
		}

		OutPayload = Cloud.Payload;
		Out.bFromCloud = true;
		// Bring the local file up to date (also repairs a damaged one).
		if (!WriteLocal(Slot, Cloud.Bytes))
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam cloud: could not update the local file of slot '%s'."), *Slot);
		}
		break;

	case ESteamCloudResolution::UseLocal:
	case ESteamCloudResolution::InSync:
	default:
		if (Cloud.bExists && !Cloud.bValid)
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam cloud: the cloud copy of slot '%s' is damaged (%s). Using the local file."), *Slot, FSteamCloudSaveFormat::DescribeDecode(Cloud.Decode));
		}

		OutPayload = Local.Payload;
		// Bring the cloud copy up to date (first upload, newer local file, or a damaged cloud copy).
		if (Resolution == ESteamCloudResolution::UseLocal && Backend->IsCloudEnabled())
		{
			bool bWritten = false;
			const FSteamResult Upload = WriteCloud(Slot, Local.Bytes, bWritten);
			if (!Upload.IsSuccess())
			{
				UE_LOG(LogSandwichSteam, Warning, TEXT("Steam cloud: slot '%s': %s"), *Slot, *Upload.Message.ToString());
			}
		}
		break;
	}

	Out.Outcome = ESteamCloudLoadOutcome::Loaded;
	Out.Result = FSteamResult::Success();
	return Out;
}

FSteamCloudLoadResult USteamCloudSubsystem::LoadBytes(const FString& Slot, TArray<uint8>& OutPayload)
{
	OutPayload.Reset();

	FSteamCloudLoadResult Out;
	if (!RequireActive(Out.Result))
	{
		return Out;
	}

	Out.Result = ValidateSlot(Slot);
	if (!Out.Result.IsSuccess())
	{
		return Out;
	}

	const FCopy Local = ReadLocal(Slot);
	const FCopy Cloud = ReadCloud(Slot);
	const USteamCloudSettings* Settings = USteamCloudSettings::Get();
	const ESteamCloudResolution Resolution = FSteamCloudSaveFormat::Resolve(Settings ? Settings->ConflictPolicy : ESteamCloudConflictPolicy::NewestWins,
		Local.bValid ? &Local.Header : nullptr, Cloud.bValid ? &Cloud.Header : nullptr);
	return Finish(Slot, Resolution, Local, Cloud, OutPayload);
}

FSteamCloudLoadResult USteamCloudSubsystem::ResolveConflict(const FString& Slot, bool bUseCloud, TArray<uint8>& OutPayload)
{
	OutPayload.Reset();

	FSteamCloudLoadResult Out;
	if (!RequireActive(Out.Result))
	{
		return Out;
	}

	Out.Result = ValidateSlot(Slot);
	if (!Out.Result.IsSuccess())
	{
		return Out;
	}

	const FCopy Local = ReadLocal(Slot);
	const FCopy Cloud = ReadCloud(Slot);

	// The game's choice, unless that copy is not usable any more: then the other one, or nothing.
	ESteamCloudResolution Resolution = ESteamCloudResolution::NothingFound;
	if (bUseCloud)
	{
		Resolution = Cloud.bValid ? ESteamCloudResolution::UseCloud : (Local.bValid ? ESteamCloudResolution::UseLocal : ESteamCloudResolution::NothingFound);
	}
	else
	{
		Resolution = Local.bValid ? ESteamCloudResolution::UseLocal : (Cloud.bValid ? ESteamCloudResolution::UseCloud : ESteamCloudResolution::NothingFound);
	}

	return Finish(Slot, Resolution, Local, Cloud, OutPayload);
}

FSteamCloudSaveResult USteamCloudSubsystem::SaveGameToCloud(const FString& Slot, USaveGame* SaveGame)
{
	if (!SaveGame)
	{
		FSteamCloudSaveResult Out;
		Out.Result = FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "CloudNoSaveGame", "There is no save game object to save."));
		return Out;
	}

	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(SaveGame, Bytes))
	{
		FSteamCloudSaveResult Out;
		Out.Result = FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "CloudSerializeFailed", "The save game object could not be serialized."));
		return Out;
	}

	return SaveBytes(Slot, Bytes);
}

FSteamCloudLoadResult USteamCloudSubsystem::LoadGameFromCloud(const FString& Slot, USaveGame*& OutSaveGame)
{
	OutSaveGame = nullptr;

	TArray<uint8> Payload;
	FSteamCloudLoadResult Out = LoadBytes(Slot, Payload);
	if (Out.Outcome == ESteamCloudLoadOutcome::Loaded)
	{
		OutSaveGame = UGameplayStatics::LoadGameFromMemory(Payload);
		if (!OutSaveGame)
		{
			Out.Outcome = ESteamCloudLoadOutcome::Corrupt;
			Out.Result = FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "CloudDeserializeFailed", "The save data could not be turned back into a save game object (did the class change?)."));
		}
	}
	return Out;
}

FSteamCloudLoadResult USteamCloudSubsystem::ResolveConflictToSaveGame(const FString& Slot, bool bUseCloud, USaveGame*& OutSaveGame)
{
	OutSaveGame = nullptr;

	TArray<uint8> Payload;
	FSteamCloudLoadResult Out = ResolveConflict(Slot, bUseCloud, Payload);
	if (Out.Outcome == ESteamCloudLoadOutcome::Loaded)
	{
		OutSaveGame = UGameplayStatics::LoadGameFromMemory(Payload);
		if (!OutSaveGame)
		{
			Out.Outcome = ESteamCloudLoadOutcome::Corrupt;
			Out.Result = FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "CloudDeserializeFailed2", "The save data could not be turned back into a save game object (did the class change?)."));
		}
	}
	return Out;
}

FSteamResult USteamCloudSubsystem::DeleteLocalSlot(const FString& Slot)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	Result = ValidateSlot(Slot);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	IFileManager::Get().Delete(*GetLocalPath(Slot), /*RequireExists*/ false, /*EvenReadOnly*/ true, /*Quiet*/ true);
	return FSteamResult::Success();
}

FSteamResult USteamCloudSubsystem::DeleteSlot(const FString& Slot)
{
	FSteamResult Result = DeleteLocalSlot(Slot);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const FString FileName = FileNameOf(Slot);
	if (Backend->FileExists(FileName))
	{
		if (!Backend->IsCloudEnabled())
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("Steam cloud: Steam Cloud is turned off, so the cloud copy of slot '%s' was not deleted."), *Slot);
		}
		else if (!Backend->RemoveFile(FileName))
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "CloudDeleteFailed", "Steam refused to delete the cloud copy. The local file was deleted."));
		}
	}
	return FSteamResult::Success();
}

void USteamCloudSubsystem::GetSlotInfo(const FString& Slot, FSteamCloudSaveInfo& OutLocal, FSteamCloudSaveInfo& OutCloud) const
{
	OutLocal = FSteamCloudSaveInfo();
	OutCloud = FSteamCloudSaveInfo();
	if (!Backend.IsValid() || !FSteamCloudSaveFormat::IsValidSlotName(Slot))
	{
		return;
	}

	OutLocal = ReadLocal(Slot).ToInfo();
	OutCloud = ReadCloud(Slot).ToInfo();
}

TArray<FSteamCloudSlotInfo> USteamCloudSubsystem::ListCloudSlots() const
{
	TArray<FSteamCloudSlotInfo> Slots;
	if (Backend.IsValid())
	{
		TArray<FSteamCloudSlotInfo> Files;
		Backend->ListFiles(Files);
		for (FSteamCloudSlotInfo& File : Files)
		{
			if (File.Slot.RemoveFromEnd(SandwichSteam::Cloud::SlotExtension))
			{
				Slots.Add(MoveTemp(File));
			}
		}
	}
	return Slots;
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamCloudSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Cloud: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!Backend.IsValid())
	{
		return Report.TrimEnd();
	}

	const USteamCloudSettings* Settings = USteamCloudSettings::Get();
	Report += FString::Printf(TEXT("  Cloud enabled: account %s, game %s\n"), Backend->IsCloudEnabledForAccount() ? TEXT("yes") : TEXT("no"), Backend->IsCloudEnabledForApp() ? TEXT("yes") : TEXT("no"));
	Report += FString::Printf(TEXT("  Conflict policy: %s\n"), *StaticEnum<ESteamCloudConflictPolicy>()->GetNameStringByValue(static_cast<int64>(Settings ? Settings->ConflictPolicy : ESteamCloudConflictPolicy::NewestWins)));

	FSteamCloudQuota Quota;
	if (Backend->GetQuota(Quota))
	{
		Report += FString::Printf(TEXT("  Quota: %lld of %lld bytes free\n"), Quota.AvailableBytes, Quota.TotalBytes);
	}

	const TArray<FSteamCloudSlotInfo> Slots = ListCloudSlots();
	Report += FString::Printf(TEXT("  Cloud slots: %d\n"), Slots.Num());
	for (const FSteamCloudSlotInfo& Slot : Slots)
	{
		FSteamCloudSaveInfo Local;
		FSteamCloudSaveInfo Cloud;
		GetSlotInfo(Slot.Slot, Local, Cloud);
		Report += FString::Printf(TEXT("    %s  %lld bytes  local %s (%lld)  cloud %s (%lld)\n"), *Slot.Slot, Slot.FileSize,
			Local.bValid ? TEXT("ok") : TEXT("missing/bad"), Local.UnixTime, Cloud.bValid ? TEXT("ok") : TEXT("missing/bad"), Cloud.UnixTime);
	}

	return Report.TrimEnd();
}
#endif // SANDWICHSTEAM_WITH_DEBUG
