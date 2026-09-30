// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamLeaderboardsLibrary.h"

FSteamLeaderboardQuery USteamLeaderboardsLibrary::MakeSteamLeaderboardTopQuery(int32 Count)
{
	FSteamLeaderboardQuery Query;
	Query.Type = ESteamLeaderboardRequestType::Global;
	Query.RangeStart = 1;
	Query.RangeEnd = FMath::Clamp(Count, 1, SandwichSteam::Leaderboards::MaxPageSize);
	return Query;
}

FSteamLeaderboardQuery USteamLeaderboardsLibrary::MakeSteamLeaderboardAroundUserQuery(int32 Before, int32 After)
{
	FSteamLeaderboardQuery Query;
	Query.Type = ESteamLeaderboardRequestType::AroundUser;
	Query.RangeStart = -FMath::Max(0, Before);
	Query.RangeEnd = FMath::Max(0, After);
	return Query;
}

FSteamLeaderboardQuery USteamLeaderboardsLibrary::MakeSteamLeaderboardFriendsQuery()
{
	FSteamLeaderboardQuery Query;
	Query.Type = ESteamLeaderboardRequestType::Friends;
	Query.RangeStart = 0;
	Query.RangeEnd = 0;
	return Query;
}

FSteamLeaderboardQuery USteamLeaderboardsLibrary::MakeSteamLeaderboardUsersQuery(const TArray<FSteamId>& Users)
{
	FSteamLeaderboardQuery Query;
	Query.Type = ESteamLeaderboardRequestType::Users;
	Query.RangeStart = 0;
	Query.RangeEnd = 0;
	Query.Users = Users;
	return Query;
}
