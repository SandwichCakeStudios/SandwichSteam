// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"
#include "Widgets/SCompoundWidget.h"

class USteamSessionsSubsystem;

/**
 * Test page for the Sessions feature (needs two machines and two Steam accounts): create a session of the first profile of the
 * App Definition, find sessions, join the first one, destroy, open the overlay invite dialog, answer a waiting join request and
 * host-travel to a map. The lobby buttons (9b) cover the party lobby, the ready state, chat, lobby data and kick, on the lobby the
 * Target button picks. The Party box (17) hosts a party session, begins / ends / follows a party match. The state block is the
 * Steam.Sessions.Dump report.
 */
class SSandwichSteamSessionsTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamSessionsTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamSessionsSubsystem* GetSessions() const;

	FText BuildStateText() const;
	FText BuildFoundText() const;
	FText BuildAppliedSettingsText() const;

	FReply OnCreate();
	FReply OnCreateWithOptions();
	FReply OnCreateBreakingRules();
	FReply OnUpdateToggleVisibility();
	FReply OnFind();
	FReply OnJoinFirst();
	FReply OnDestroy();
	FReply OnInviteOverlay();
	FReply OnAccept();
	FReply OnDecline();
	FReply OnHostTravel();

	// Lobby extras (9b)
	FReply OnLobbyCreate();
	FReply OnLobbyFind();
	FReply OnLobbyJoinFirst();
	FReply OnLobbyLeave();
	FReply OnToggleReady();
	FReply OnSendChat();
	FReply OnSetLobbyData();
	FReply OnKickOther();
	FReply OnForgiveAll();

	FText BuildLobbiesText() const;

	// Party lobby (Phase 17)
	FText BuildPartyText() const;
	FReply OnCycleTarget();
	FReply OnHostPartySession();
	FReply OnBeginListenMatch();
	FReply OnEndMatch();
	FReply OnFollowMatch();
	FReply OnPartyInviteOverlay();

	TWeakObjectPtr<UObject> WorldContext;
	FText Status;

	/** Result of the last search, shared with the delegates so a closed page does not leave a dangling capture. */
	TSharedRef<TArray<FSteamSessionResult>> Found = MakeShared<TArray<FSteamSessionResult>>();
	TSharedRef<TArray<FSteamLobbyInfo>> FoundLobbies = MakeShared<TArray<FSteamLobbyInfo>>();
	bool bReadyState = false;
	int32 ChatCounter = 0;
	/** Alternates between Public and Friends Only each time "Update: toggle visibility" is pressed. */
	bool bToggleToFriendsOnly = true;
	/** Which lobby the lobby buttons work on (switched with the Target button of the Party box). */
	ESteamLobbyTarget LobbyTarget = ESteamLobbyTarget::Active;
	TSharedRef<FText> AsyncStatus = MakeShared<FText>();
};
