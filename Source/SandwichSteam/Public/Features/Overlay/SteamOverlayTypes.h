// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamOverlayTypes.generated.h"

/** A page of the Steam overlay. */
UENUM(BlueprintType)
enum class ESteamOverlayDialog : uint8
{
	/** Friends list. */
	Friends,
	/** Steam Community hub of this game. */
	Community,
	/** Players you recently played with. */
	Players,
	/** Steam overlay settings. */
	Settings,
	/** Official group of this game. */
	OfficialGameGroup,
	/** Statistics of the local user for this game. */
	Stats,
	/** Achievements of the local user for this game. */
	Achievements,
	/** Store page. Without an argument: this game's store page. */
	Store,
	/** Invite dialog for a lobby. Needs the lobby ID (Open Target Dialog). */
	InviteDialog,
	/** Web page in the overlay browser. Needs a URL (Open Web Page). */
	WebPage,
	/** Steam profile of a user. Needs a Steam ID (Open Target Dialog). */
	UserProfile,
	/** Chat with a user. Needs a Steam ID (Open Target Dialog). */
	UserChat,
	/** Add friend dialog for a user. Needs a Steam ID (Open Target Dialog). */
	AddFriend
};

/** What happens to the shopping cart when the store page opens. */
UENUM(BlueprintType)
enum class ESteamOverlayStoreFlag : uint8
{
	None,
	/** Add the app to the cart. */
	AddToCart,
	/** Add the app to the cart and show the cart. */
	AddToCartAndShow
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamOverlayActivated, bool, bActive);
