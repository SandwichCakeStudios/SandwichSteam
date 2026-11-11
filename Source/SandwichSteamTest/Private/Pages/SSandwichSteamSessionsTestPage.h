// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Pages/SandwichSteamSessionsTestListener.h"
#include "SteamLobbyTypes.h"
#include "SteamSessionTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class USteamSessionsSubsystem;

/**
 * Test page for the Sessions feature (needs two machines and two Steam accounts): create a session of the first profile of the
 * App Definition, find sessions, join the first one, destroy, open the overlay invite dialog, answer a waiting join request and
 * host-travel to a map. The join events box lists every join request from Steam (invite, Join Game, launch) and every finished
 * join, so an invite accepted on this machine shows here. The lobby buttons (9b) cover the party lobby, the ready state, chat,
 * lobby data and kick, on the lobby the Target button picks. The Party box (17) hosts a party session, begins / ends / follows a
 * party match. The state block is the Steam.Sessions.Dump report.
 */
class SSandwichSteamSessionsTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamSessionsTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	virtual ~SSandwichSteamSessionsTestPage() override;

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	USteamSessionsSubsystem* GetSessions() const;

	/** Binds the listener to the current Sessions subsystem (it changes with PIE sessions and travel). */
	void UpdateBinding();
	void Unbind();

	void HandleJoinRequested(const FSteamJoinRequest& Request);
	void HandleJoinFinished(const FSteamResult& Result, const FString& ConnectString);
	void AddJoinEvent(const FString& Line);
	FText BuildJoinEventsText() const;
	FReply OnClearJoinEvents();

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

	TStrongObjectPtr<USandwichSteamSessionsTestListener> Listener;
	TWeakObjectPtr<USteamSessionsSubsystem> BoundSessions;
	/** Join requests and finished joins, newest first, capped at MaxJoinEvents. */
	TArray<FString> JoinEvents;
	static constexpr int32 MaxJoinEvents = 8;
};
