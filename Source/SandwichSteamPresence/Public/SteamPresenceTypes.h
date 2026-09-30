// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "SteamPresenceTypes.generated.h"

/** Called when the rich presence of a friend changed. Coalesced: one call per friend and frame at most. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamFriendPresenceChanged, FSteamId, Friend);

/** The rich presence of a user as Steam reports it. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMPRESENCE_API FSteamPresenceValues
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Every key and value the user has set. steam_display holds the localized status text."))
	TMap<FString, FString> Values;
};
