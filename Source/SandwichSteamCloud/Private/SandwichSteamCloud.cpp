// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamCloud.h"
#include "Debug/SteamDebugSection.h"
#include "Misc/DateTime.h"
#include "SteamCloudSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	FString SlotArg(const TArray<FString>& Args)
	{
		return Args.IsEmpty() ? FString(TEXT("DebugSlot")) : Args[0];
	}

	void DumpCloud(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamCloudSubsystem* Cloud = FSteamDebugCommandSet::FindFeature<USteamCloudSubsystem>(World, Output))
		{
			Output.Log(*Cloud->BuildDebugString());
		}
	}

	FString PayloadToString(const TArray<uint8>& Payload)
	{
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
		return FString(Converted.Length(), Converted.Get());
	}

	/** Steam.Cloud.Roundtrip [Slot]: saves a text payload and loads it back. Then try Steam.Cloud.DeleteLocal and Steam.Cloud.Read. */
	void Roundtrip(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamCloudSubsystem* Cloud = FSteamDebugCommandSet::FindFeature<USteamCloudSubsystem>(World, Output);
		if (!Cloud)
		{
			return;
		}

		const FString Slot = SlotArg(Args);
		const FString Text = FString::Printf(TEXT("Sandwich Steam cloud test %s"), *FDateTime::UtcNow().ToIso8601());
		const FTCHARToUTF8 Utf8(*Text);
		const TConstArrayView<uint8> Payload(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

		const FSteamCloudSaveResult Saved = Cloud->SaveBytes(Slot, Payload);
		Output.Logf(TEXT("Save: %s (local %s, cloud %s)"), Saved.Result.IsSuccess() ? TEXT("ok") : *Saved.Result.Message.ToString(),
			Saved.bSavedLocal ? TEXT("yes") : TEXT("no"), Saved.bSavedCloud ? TEXT("yes") : TEXT("no (cloud off or failed)"));

		TArray<uint8> Loaded;
		const FSteamCloudLoadResult Load = Cloud->LoadBytes(Slot, Loaded);
		const bool bSame = Loaded.Num() == Payload.Num() && FMemory::Memcmp(Loaded.GetData(), Payload.GetData(), Payload.Num()) == 0;
		Output.Logf(TEXT("Load: outcome %d, %s"), static_cast<int32>(Load.Outcome), bSame ? TEXT("payload identical") : *Load.Result.Message.ToString());
	}

	/** Steam.Cloud.Read [Slot]: loads a slot and prints its text. */
	void ReadSlot(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamCloudSubsystem* Cloud = FSteamDebugCommandSet::FindFeature<USteamCloudSubsystem>(World, Output);
		if (!Cloud)
		{
			return;
		}

		TArray<uint8> Loaded;
		const FSteamCloudLoadResult Load = Cloud->LoadBytes(SlotArg(Args), Loaded);
		Output.Logf(TEXT("Read: outcome %d (0 Loaded, 1 NotFound, 2 Conflict, 3 Corrupt), from cloud %s, %s"), static_cast<int32>(Load.Outcome), Load.bFromCloud ? TEXT("yes") : TEXT("no"),
			Load.Result.IsSuccess() ? *PayloadToString(Loaded) : *Load.Result.Message.ToString());
	}

	/** Steam.Cloud.DeleteLocal [Slot]: deletes only the local file, to prove the cloud copy restores it. */
	void DeleteLocal(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		if (USteamCloudSubsystem* Cloud = FSteamDebugCommandSet::FindFeature<USteamCloudSubsystem>(World, Output))
		{
			const FSteamResult Result = Cloud->DeleteLocalSlot(SlotArg(Args));
			Output.Logf(TEXT("DeleteLocal: %s"), Result.IsSuccess() ? TEXT("deleted (Steam.Cloud.Read should restore it from the cloud)") : *Result.Message.ToString());
		}
	}

	/** Steam.Cloud.Delete [Slot]: deletes the local file and the cloud copy. */
	void DeleteSlot(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		if (USteamCloudSubsystem* Cloud = FSteamDebugCommandSet::FindFeature<USteamCloudSubsystem>(World, Output))
		{
			const FSteamResult Result = Cloud->DeleteSlot(SlotArg(Args));
			Output.Logf(TEXT("Delete: %s"), Result.IsSuccess() ? TEXT("deleted") : *Result.Message.ToString());
		}
	}

	const FName DebugSectionId(TEXT("Cloud"));

	FString ReportCloud(UWorld* World)
	{
		const USteamCloudSubsystem* Cloud = SandwichSteam::Debug::FindFeatureSubsystem<USteamCloudSubsystem>(World);
		return Cloud ? Cloud->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamCloudModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamCloud", "DebugTitle", "Cloud");
	Section.Order = 100;
	Section.BuildReport = &ReportCloud;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Cloud.Dump"),
		TEXT("Prints whether Steam Cloud is on, the quota, the conflict policy and the cloud slots with the state of both copies."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpCloud));

	Commands.Add(TEXT("Steam.Cloud.Roundtrip"),
		TEXT("Saves a test payload into a slot and loads it back: Steam.Cloud.Roundtrip [Slot] (default DebugSlot)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&Roundtrip));

	Commands.Add(TEXT("Steam.Cloud.Read"),
		TEXT("Loads a slot and prints its text: Steam.Cloud.Read [Slot]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ReadSlot));

	Commands.Add(TEXT("Steam.Cloud.DeleteLocal"),
		TEXT("Deletes only the local file of a slot: Steam.Cloud.DeleteLocal [Slot]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DeleteLocal));

	Commands.Add(TEXT("Steam.Cloud.Delete"),
		TEXT("Deletes the local file and the cloud copy of a slot: Steam.Cloud.Delete [Slot]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DeleteSlot));
#endif
}

void FSandwichSteamCloudModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamCloudModule, SandwichSteamCloud)
