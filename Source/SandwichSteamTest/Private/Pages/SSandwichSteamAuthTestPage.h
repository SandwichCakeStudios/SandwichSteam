// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Pages/SandwichSteamAuthTestListener.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class APlayerState;
class USteamAuthSubsystem;
struct FSteamAuthResult;

/**
 * Test page for the Auth feature (needs Enable Steam Authentication + Configure Steam, and a second machine with a second Steam
 * account joining this host; see Systems/Auth.md): the Steam.Auth.Dump report, a live list of the On Steam Auth Result /
 * On Steam Auth Failed events, and the players of the game with their status and a Kick button (Kick Steam Player).
 */
class SSandwichSteamAuthTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamAuthTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	virtual ~SSandwichSteamAuthTestPage() override;

	void Construct(const FArguments& InArgs);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

private:
	/** Player rows shown at once (fixed slots, hidden when empty). */
	static constexpr int32 MaxRows = 8;
	/** Events kept in the list, newest first. */
	static constexpr int32 MaxEvents = 20;

	struct FPlayerRow
	{
		FSteamId Player;
		TWeakObjectPtr<APlayerState> PlayerState;
		bool bLocal = false;
	};

	USteamAuthSubsystem* GetAuth() const;

	/** Binds the listener to the current Auth subsystem, and unbinds it from the old one when the game instance changed. */
	void UpdateBinding();
	void Unbind();

	void HandleResult(const FSteamAuthResult& Result);
	void HandleFailed(FSteamId Player);
	void AddEvent(const FString& Line);

	FText BuildStateText() const;
	FText BuildEventsText() const;
	FText BuildRowText(int32 Index) const;
	FText BuildRoleText() const;

	FReply OnKick(int32 Index);
	FReply OnClearEvents();

	TWeakObjectPtr<UObject> WorldContext;
	TStrongObjectPtr<USandwichSteamAuthTestListener> Listener;
	TWeakObjectPtr<USteamAuthSubsystem> BoundAuth;
	TArray<FPlayerRow> Rows;
	TArray<FString> Events;
	FText Status;
};
