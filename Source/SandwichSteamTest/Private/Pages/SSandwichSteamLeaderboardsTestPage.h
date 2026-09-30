// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "SteamLeaderboardTypes.h"
#include "Widgets/SCompoundWidget.h"

class USteamLeaderboardsSubsystem;

/** Test page for the Leaderboards feature: upload a score, top 10, around me, friends, and the de-dup check (two identical downloads, one Steam call). */
class SSandwichSteamLeaderboardsTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamLeaderboardsTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamLeaderboardsSubsystem* GetLeaderboards() const;

	/** Name of the first leaderboard of the App Definition, or None. */
	FName GetBoardName() const;

	FText BuildEntriesText() const;

	FReply OnUpload();
	FReply OnDownload(ESteamLeaderboardRequestType Type);
	FReply OnDownloadTwice();
	void HandleUpload(const FSteamResult& Result, const FSteamLeaderboardUploadResult& Upload);
	void HandleEntries(const FSteamResult& Result, const TArray<FSteamLeaderboardEntry>& NewEntries);

	TWeakObjectPtr<UObject> WorldContext;
	TArray<FSteamLeaderboardEntry> Entries;
	FText Status;
};
