// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Subsystems/SubsystemCollection.h"

bool USteamFeatureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer) || IsRunningCommandlet())
	{
		return false;
	}

	const ESteamFeatureScope Scope = GetFeatureScope();
	const bool bDedicatedServer = IsRunningDedicatedServer();
	if ((bDedicatedServer && Scope == ESteamFeatureScope::ClientOnly) || (!bDedicatedServer && Scope == ESteamFeatureScope::ServerOnly))
	{
		return false;
	}

	const USteamToolSettings* Settings = USteamToolSettings::Get();
	return !Settings || Settings->IsFeatureEnabled(GetFeatureTag());
}

void USteamFeatureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Core = Collection.InitializeDependency<USteamCoreSubsystem>();
	if (!Core)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("%s inactive: Steam core subsystem is not available."), *GetClass()->GetName());
		return;
	}

	Core->OnSteamStateChanged.AddDynamic(this, &USteamFeatureSubsystem::HandleSteamStateChanged);

	ApplySteamState(Core->GetSteamState());

	if (!bFeatureActive)
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("%s inactive (Steam state: %s)."), *GetClass()->GetName(), LexToString(Core->GetSteamState()));
	}
}

void USteamFeatureSubsystem::Deinitialize()
{
	if (Core)
	{
		Core->OnSteamStateChanged.RemoveDynamic(this, &USteamFeatureSubsystem::HandleSteamStateChanged);
	}

	SetFeatureActive(false);
	Core = nullptr;

	Super::Deinitialize();
}

bool USteamFeatureSubsystem::RequireActive(FSteamResult& OutResult) const
{
	check(IsInGameThread());

	if (bFeatureActive)
	{
		OutResult = FSteamResult::Success();
		return true;
	}

	const ESteamState State = Core ? Core->GetSteamState() : ESteamState::Unavailable;

	FGameplayTag ErrorTag = SteamGameplayTags::Error_Unavailable;
	switch (State)
	{
	case ESteamState::WaitingForSteam:
	case ESteamState::Ready: // Ready but the feature failed to initialize.
		ErrorTag = SteamGameplayTags::Error_NotInitialized;
		break;
	case ESteamState::Offline:
		ErrorTag = SteamGameplayTags::Error_Offline;
		break;
	default:
		break;
	}

	OutResult = FSteamResult::Failure(ErrorTag,
		FText::Format(NSLOCTEXT("SandwichSteam", "FeatureInactive", "{0} is not active (Steam state: {1})."),
			FText::FromString(GetClass()->GetName()), FText::FromString(LexToString(State))));
	return false;
}

void USteamFeatureSubsystem::HandleSteamStateChanged(ESteamState NewState, ESteamState /*OldState*/)
{
	ApplySteamState(NewState);
}

void USteamFeatureSubsystem::ApplySteamState(ESteamState State)
{
	SetFeatureActive(State == ESteamState::Ready);
}

void USteamFeatureSubsystem::SetFeatureActive(bool bActive)
{
	if (bActive == bFeatureActive)
	{
		return;
	}

	if (bActive)
	{
		if (!InitializeFeature())
		{
			UE_LOG(LogSandwichSteam, Log, TEXT("%s inactive: InitializeFeature() failed."), *GetClass()->GetName());
			return;
		}
		bFeatureActive = true;
	}
	else
	{
		ShutdownFeature();
		bFeatureActive = false;
	}

	UE_LOG(LogSandwichSteam, Log, TEXT("%s %s."), *GetClass()->GetName(), bFeatureActive ? TEXT("active") : TEXT("inactive"));
	OnFeatureActiveChanged.Broadcast(bFeatureActive);
}
