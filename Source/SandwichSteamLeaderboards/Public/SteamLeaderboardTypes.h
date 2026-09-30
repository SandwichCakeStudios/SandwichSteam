// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "SteamLeaderboardTypes.generated.h"

namespace SandwichSteam::Leaderboards
{
	/** Most entries one download returns (Steam's own limit per request is larger, this keeps pages small). */
	constexpr int32 MaxPageSize = 100;

	/** Most score details (extra int32 values stored next to a score) Steam accepts. */
	constexpr int32 MaxDetails = 64;
}

/** What happens when the uploaded score is not better than the stored one. */
UENUM(BlueprintType)
enum class ESteamLeaderboardUploadMethod : uint8
{
	/** Only store the score when it is better than the current one. */
	KeepBest UMETA(DisplayName = "Keep Best"),
	/** Always store the score, even when it is worse. */
	ForceUpdate UMETA(DisplayName = "Force Update")
};

/** Which entries a download returns. */
UENUM(BlueprintType)
enum class ESteamLeaderboardRequestType : uint8
{
	/** Ranks Range Start..Range End from the top (1 is the best). */
	Global,
	/** Ranks around the local user: Range Start is the offset above (negative, e.g. -4), Range End the offset below (e.g. 5). */
	AroundUser,
	/** Entries of the local user's friends (the range is ignored). */
	Friends,
	/** Entries of the listed users only (the range is ignored). */
	Users
};

/** One row of a leaderboard. Resolve avatars and names lazily with the User feature. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMLEADERBOARDS_API FSteamLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam ID of the player."))
	FSteamId SteamId;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Global rank of the entry (1 is the best)."))
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "The score."))
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Extra values that were uploaded with the score (at most 64)."))
	TArray<int32> Details;
};

/** What Steam answered to an upload. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMLEADERBOARDS_API FSteamLeaderboardUploadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "The score that was uploaded."))
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the stored score changed. False when Keep Best found a better score already stored."))
	bool bScoreChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Global rank after the upload."))
	int32 NewRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Global rank before the upload. 0 when the player had no entry."))
	int32 PreviousRank = 0;
};

/** Describes which entries to download. Build one with the Make Steam Leaderboard * Query functions or fill it directly. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMLEADERBOARDS_API FSteamLeaderboardQuery
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Which entries to download."))
	ESteamLeaderboardRequestType Type = ESteamLeaderboardRequestType::Global;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Global: first rank (1 is the best). Around User: offset above the local user (negative). Ignored for Friends and Users."))
	int32 RangeStart = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "Global: last rank. Around User: offset below the local user. At most 100 entries are returned. Ignored for Friends and Users."))
	int32 RangeEnd = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (EditCondition = "Type == ESteamLeaderboardRequestType::Users", ToolTip = "Users to look up (Users only, at most 100)."))
	TArray<FSteamId> Users;
};

/** C++ completion of an upload. The result struct is only meaningful on success. Runs on the game thread, never after the feature shut down. */
DECLARE_DELEGATE_TwoParams(FSteamLeaderboardUploadDelegate, const FSteamResult& /*Result*/, const FSteamLeaderboardUploadResult& /*Upload*/);

/** C++ completion of a download. Runs on the game thread, never after the feature shut down. */
DECLARE_DELEGATE_TwoParams(FSteamLeaderboardDownloadDelegate, const FSteamResult& /*Result*/, const TArray<FSteamLeaderboardEntry>& /*Entries*/);
