// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"
#include "SteamSessionsSettings.generated.h"

/** Project Settings > Plugins > Sandwich Steam - Sessions */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sandwich Steam - Sessions"))
class SANDWICHSTEAMSESSIONS_API USteamSessionsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const USteamSessionsSettings* Get();

	//~ Begin UDeveloperSettings
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	//~ End UDeveloperSettings

	UPROPERTY(Config, EditAnywhere, Category = "Joining", meta = (ToolTip = "What happens when the player accepts an invite or Join Game while already in a session."))
	ESteamJoinInMatchPolicy JoinWhileInMatch = ESteamJoinInMatchPolicy::AskGame;

	UPROPERTY(Config, EditAnywhere, Category = "Joining", meta = (ToolTip = "Join automatically when an invite, Join Game or launch request arrives. Turn off to let the game decide: On Join Requested fires and the game calls Accept Join Request."))
	bool bAutoJoinRequests = true;

	UPROPERTY(Config, EditAnywhere, Category = "Joining", meta = (ToolTip = "Travel to the host (ClientTravel) as soon as a join succeeds. Turn off to travel yourself, for example after a loading screen."))
	bool bAutoTravelAfterJoin = true;

	UPROPERTY(Config, EditAnywhere, Category = "Joining", meta = (ClampMin = "0.0", ClampMax = "60.0", ToolTip = "A second request for the same lobby within this many seconds is dropped. Steam and the Online Subsystem can both report the same invite."))
	float DuplicateRequestSeconds = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Joining", meta = (ClampMin = "0.0", ClampMax = "120.0", ToolTip = "Seconds to wait after Steam is ready for a player controller before a launch join request is given up. It travels once the first local player exists."))
	float LaunchJoinWaitSeconds = 30.0f;

	/** Real enforcement of the player count: the Sessions guardrails (profile Min/Max Players) are only checked on the host and can be skipped by a modified client. */
	UPROPERTY(Config, EditAnywhere, Category = "Hosting", meta = (ToolTip = "When hosting, set AGameSession::MaxPlayers of the map's game mode to the applied Max Players (again after every travel), so AGameSession::ApproveLogin refuses joins past it. Turn off if your project sets its own GameSession max players."))
	bool bApplyMaxPlayersToGameSession = true;

	UPROPERTY(Config, EditAnywhere, Category = "Party", meta = (ToolTip = "Lobby type of the party while a party match runs. Private hides it from search and friends, but invites still work. End Steam Party Match restores the type it had before."))
	ESteamPartyMatchVisibility PartyMatchVisibility = ESteamPartyMatchVisibility::Private;

	UPROPERTY(Config, EditAnywhere, Category = "Party", meta = (ToolTip = "No one can join the party while a match runs (Set Steam Lobby Joinable false), not even by invite. End Steam Party Match opens it again."))
	bool bClosePartyDuringMatch = false;

	UPROPERTY(Config, EditAnywhere, Category = "Party", meta = (ToolTip = "Members join the owner's match by themselves when Begin Steam Party Match is called. Turn off to decide in the game: On Party Match Started fires and the game calls Follow Steam Party Match."))
	bool bAutoFollowPartyMatch = true;

	UPROPERTY(Config, EditAnywhere, Category = "Party", meta = (ToolTip = "When the host's session ends (Destroy Steam Session or a lost session), the party match ends and the party opens again. Only for matches with a Session Lobby (Host Steam Party Session)."))
	bool bAutoEndPartyMatch = true;
};
