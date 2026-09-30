// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "SteamAchievementTypes.generated.h"

class UTexture2D;

/** Everything the game needs to show one achievement in a list. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMACHIEVEMENTS_API FSteamAchievementInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Gameplay tag of the achievement (empty when the achievement is not in the Steam App Definition)."))
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "API Name of the achievement in Steamworks."))
	FName ApiName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Localized name from Steam."))
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Localized description from Steam. Hidden achievements have no description until unlocked."))
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True for hidden achievements."))
	bool bHidden = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the local user unlocked the achievement."))
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "When the achievement was unlocked (UTC). 1970-01-01 while locked."))
	FDateTime UnlockTime = FDateTime::FromUnixTimestamp(0);

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Current value of the progress stat. 0 for achievements without progress."))
	int32 ProgressCurrent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Value at which the achievement completes. 0 for achievements without progress."))
	int32 ProgressMax = 0;
};

/** An achievement was unlocked (also fires when it was unlocked outside the game, Steam permitting). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamAchievementUnlocked, FGameplayTag, AchievementTag, FName, ApiName);

/** Progress towards an achievement was shown by Steam. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamAchievementProgress, FGameplayTag, AchievementTag, FName, ApiName, int32, Current, int32, Max);

/** C++ completion of an icon request. The texture is null on failure. Runs on the game thread. */
DECLARE_DELEGATE_TwoParams(FSteamAchievementIconDelegate, const FSteamResult& /*Result*/, UTexture2D* /*Icon*/);

/** C++ completion of a global percentages request. Runs on the game thread. */
DECLARE_DELEGATE_OneParam(FSteamAchievementPercentagesDelegate, const FSteamResult& /*Result*/);
