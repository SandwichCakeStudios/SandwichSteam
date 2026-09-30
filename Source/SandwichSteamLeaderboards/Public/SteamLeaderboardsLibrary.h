// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamLeaderboardTypes.h"
#include "SteamLeaderboardsLibrary.generated.h"

/** Helpers that build the query of the Download Steam Leaderboard Entries node. */
UCLASS()
class SANDWICHSTEAMLEADERBOARDS_API USteamLeaderboardsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|Leaderboards", meta = (ToolTip = "Query for the best entries: ranks 1 to Count (at most 100)."))
	static FSteamLeaderboardQuery MakeSteamLeaderboardTopQuery(int32 Count = 10);

	UFUNCTION(BlueprintPure, Category = "Steam|Leaderboards", meta = (ToolTip = "Query for the entries around the local user: Before ranks above and After ranks below (at most 100 in total)."))
	static FSteamLeaderboardQuery MakeSteamLeaderboardAroundUserQuery(int32 Before = 4, int32 After = 5);

	UFUNCTION(BlueprintPure, Category = "Steam|Leaderboards", meta = (ToolTip = "Query for the entries of the friends of the local user."))
	static FSteamLeaderboardQuery MakeSteamLeaderboardFriendsQuery();

	UFUNCTION(BlueprintPure, Category = "Steam|Leaderboards", meta = (ToolTip = "Query for the entries of the listed users (at most 100)."))
	static FSteamLeaderboardQuery MakeSteamLeaderboardUsersQuery(const TArray<FSteamId>& Users);
};
