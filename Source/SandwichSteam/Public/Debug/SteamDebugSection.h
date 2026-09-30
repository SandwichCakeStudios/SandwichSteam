// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if SANDWICHSTEAM_WITH_DEBUG

#include "Engine/GameInstance.h"
#include "Engine/World.h"

class SWidget;
class UWorld;

/** A button of a debug section. Runs on the game thread with the world the panel shows. */
struct FSteamDebugAction
{
	FText Label;
	FText ToolTip;
	TFunction<void(UWorld*)> Execute;
};

/**
 * One block of the Steam debug panel (editor tab and the runtime overlay). Each module registers its own sections,
 * so the panel only lists the features that are installed. The panel refreshes BuildReport twice a second.
 */
struct FSteamDebugSection
{
	/** Unique id. Registering the same id again replaces the section. */
	FName Id;

	FText Title;

	/** Lower values are listed first. */
	int32 Order = 0;

	/** Multi-line state text (usually the feature's BuildDebugString). World may be null. */
	TFunction<FString(UWorld*)> BuildReport;

	/** Optional. Adds the buttons that are useful right now (may depend on the state, for example one per achievement). */
	TFunction<void(UWorld*, TArray<FSteamDebugAction>&)> BuildActions;
};

namespace SandwichSteam::Debug
{
	/** Adds or replaces a section. Game thread. */
	SANDWICHSTEAM_API void RegisterSection(FSteamDebugSection Section);

	/** Removes a section. Call from ShutdownModule for every registered id. */
	SANDWICHSTEAM_API void UnregisterSection(FName Id);

	/** Snapshot of the registered sections, sorted by Order. */
	SANDWICHSTEAM_API TArray<FSteamDebugSection> GetSections();

	/** The running game world (Standalone or PIE) that owns Steam, else the first game world with a Steam core, else nullptr. */
	SANDWICHSTEAM_API UWorld* FindSteamOwnerWorld();

	/**
	 * Builds the live debug panel. WorldGetter is called every refresh; it may return nullptr (the panel then explains
	 * how to get live data). Include Widgets/SWidget.h to use the result.
	 */
	SANDWICHSTEAM_API TSharedRef<SWidget> CreateDebugPanel(TFunction<UWorld*()> WorldGetter);

	/** Feature subsystem of a world, or nullptr. Helper for section lambdas. */
	template <typename TSubsystem>
	TSubsystem* FindFeatureSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
	}

	/** Text a section shows when its feature does not exist in the world. */
	SANDWICHSTEAM_API FString GetFeatureMissingText();
}

#endif // SANDWICHSTEAM_WITH_DEBUG
