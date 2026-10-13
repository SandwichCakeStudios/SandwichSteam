// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "SteamAuthTypes.generated.h"

/** What happens to a player who fails Steam authentication. */
UENUM(BlueprintType)
enum class ESteamAuthFailurePolicy : uint8
{
	Kick UMETA(DisplayName = "Kick", ToolTip = "The engine kicks players who fail. Nothing is overridden; On Steam Auth Result still reports the failure."),
	Custom UMETA(DisplayName = "Custom", ToolTip = "Nobody is kicked automatically. On Steam Auth Failed fires and your game decides, for example with Kick Steam Player.")
};

/** Steam's answer for one joining player. 1:1 with the Online Subsystem Steam response codes. */
UENUM(BlueprintType)
enum class ESteamAuthResponse : uint8
{
	OK UMETA(DisplayName = "OK", ToolTip = "The ticket is valid and the player owns the game."),
	NotConnectedToSteam UMETA(DisplayName = "Not Connected To Steam", ToolTip = "The player's Steam client is not connected to Steam."),
	NoOwnership UMETA(DisplayName = "No Ownership", ToolTip = "The player does not own this App ID."),
	VACBanned UMETA(DisplayName = "VAC Banned", ToolTip = "The player is VAC banned for this game."),
	LoggedInElsewhere UMETA(DisplayName = "Logged In Elsewhere", ToolTip = "The account is logged in on another machine."),
	VACCheckTimeout UMETA(DisplayName = "VAC Check Timeout", ToolTip = "Steam could not finish the VAC check in time."),
	TicketCancelled UMETA(DisplayName = "Ticket Cancelled", ToolTip = "The player cancelled the ticket (for example by leaving)."),
	TicketAlreadyUsed UMETA(DisplayName = "Ticket Already Used", ToolTip = "The ticket was already used by another connection."),
	TicketInvalid UMETA(DisplayName = "Ticket Invalid", ToolTip = "The ticket did not come from this player or is broken."),
	PublisherBanned UMETA(DisplayName = "Publisher Banned", ToolTip = "The publisher banned the player from this game."),
	FailedToCreateUser UMETA(DisplayName = "Failed To Create User", ToolTip = "The engine could not create the user for the result."),
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "A code this plugin does not know (newer engine or SDK).")
};

/** Where a player is in the Steam check, as seen by the host. */
UENUM(BlueprintType)
enum class ESteamAuthStatus : uint8
{
	NotChecked UMETA(DisplayName = "Not Checked", ToolTip = "No result and no login seen for this player on this host (Steam auth off, not the host, or the player is not connected)."),
	Pending UMETA(DisplayName = "Pending", ToolTip = "The player is logged in and Steam has not answered yet. Steam answers shortly after the login."),
	Passed UMETA(DisplayName = "Passed", ToolTip = "Steam accepted the player."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "Steam refused the player, or the player joined without authentication.")
};

/** One Steam authentication result on the host. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAMAUTH_API FSteamAuthResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Auth", meta = (ToolTip = "The player Steam checked."))
	FSteamId Player;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Auth", meta = (ToolTip = "True when Steam accepted the player."))
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Auth", meta = (ToolTip = "Steam's reason. OK when the player passed."))
	ESteamAuthResponse Response = ESteamAuthResponse::Unknown;
};

/** Steam answered for a joining player (host only). Fires on the game thread. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAuthResultEvent, const FSteamAuthResult&, Result);

/** A player failed Steam authentication and the Custom policy is on: nobody was kicked, the game decides. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAuthFailedEvent, FSteamId, Player);
