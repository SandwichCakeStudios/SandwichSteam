// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "Features/User/SteamUserTypes.h"
#include "SteamAchievementTypes.h"
#include "Widgets/SCompoundWidget.h"

class USteamAchievementsSubsystem;
class USteamStatsSubsystem;

/** Test page for the Stats and Achievements features: live stat values, +1 buttons, unlock / clear, global percentages. */
class SSandwichSteamStatsTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamStatsTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamStatsSubsystem* GetStats() const;
	USteamAchievementsSubsystem* GetAchievements() const;

	FText BuildStatsText() const;
	FText BuildAchievementsText() const;

	FReply OnAddToStats();
	FReply OnStoreNow();
	FReply OnRefreshAchievements();
	FReply OnUnlockNext();
	FReply OnClearLast();
	FReply OnRequestPercentages();
	void HandlePercentages(const FSteamResult& Result);
	FReply OnByNameCheck();

	void ReportResult(const FText& Action, const FSteamResult& Result);

	TWeakObjectPtr<UObject> WorldContext;
	TArray<FSteamAchievementInfo> Infos;
	FText Status;
};
