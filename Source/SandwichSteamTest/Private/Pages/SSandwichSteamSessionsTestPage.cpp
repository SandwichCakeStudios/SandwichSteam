// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamSessionsTestPage.h"
#include "Data/SteamAppDefinition.h"
#include "Engine/World.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamSessionsSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestSessions"

void SSandwichSteamSessionsTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Needs the OnlineSubsystemSteam setup (Configure Steam), a session profile in the App Definition and, to join, a second machine with a second Steam account.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Sessions", "Sessions")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamSessionsSubsystem* Sessions = GetSessions();
			return Sessions ? SandwichSteamTest::BoolText(Sessions->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("InSession", "In a session"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamSessionsSubsystem* Sessions = GetSessions();
			return Sessions ? SandwichSteamTest::BoolText(Sessions->IsInSession()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildStateText(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildFoundText(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildAppliedSettingsText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Create", "Create (first profile)"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnCreate)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("CreateWithOptions", "Create with options (Friends Only, Max-1, 'OST test')"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnCreateWithOptions)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("CreateBreakingRules", "Create breaking rules (must fail)"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnCreateBreakingRules)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("UpdateToggleVisibility", "Update: toggle visibility"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnUpdateToggleVisibility)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("HostTravel", "Host travel to this map"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnHostTravel)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Find", "Find"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnFind)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Join", "Join first result"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnJoinFirst)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Destroy", "Destroy"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnDestroy)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Invite", "Invite overlay"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnInviteOverlay)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Accept", "Accept request"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnAccept)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Decline", "Decline request"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnDecline)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Lobby", "Lobby (works on the session lobby or a lobby only lobby)")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildLobbiesText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LobbyCreate", "Create lobby only (first profile)"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnLobbyCreate)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LobbyFind", "Find lobbies"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnLobbyFind)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LobbyJoin", "Join first lobby"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnLobbyJoinFirst)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LobbyLeave", "Leave lobby only lobby"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnLobbyLeave)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Ready", "Toggle ready"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnToggleReady)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Chat", "Send chat"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnSendChat)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("LobbyData", "Set lobby data (owner)"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnSetLobbyData)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Kick", "Kick first other member (owner)"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnKickOther)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Forgive", "Forgive all (owner)"), FOnClicked::CreateSP(this, &SSandwichSteamSessionsTestPage::OnForgiveAll)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)))
			.Text_Lambda([this] { return AsyncStatus->IsEmpty() ? Status : *AsyncStatus; })
		]
	];
}

USteamSessionsSubsystem* SSandwichSteamSessionsTestPage::GetSessions() const
{
	return USteamSessionsSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamSessionsTestPage::BuildStateText() const
{
#if SANDWICHSTEAM_WITH_DEBUG
	const USteamSessionsSubsystem* Sessions = GetSessions();
	return Sessions ? FText::FromString(Sessions->BuildDebugString()) : FText::GetEmpty();
#else
	return FText::GetEmpty();
#endif
}

FText SSandwichSteamSessionsTestPage::BuildFoundText() const
{
	if (Found->IsEmpty())
	{
		return LOCTEXT("NoneFound", "No search results yet.");
	}

	FString Text = FString::Printf(TEXT("Last search: %d session(s)\n"), Found->Num());
	for (int32 Index = 0; Index < Found->Num(); ++Index)
	{
		const FSteamSessionResult& Session = (*Found)[Index];
		Text += FString::Printf(TEXT("  [%d] host %s, %d/%d free, ping %d ms\n"), Index, *Session.OwnerName, Session.OpenSlots, Session.MaxPlayers, Session.PingMs);
	}
	return FText::FromString(Text.TrimEnd());
}

FReply SSandwichSteamSessionsTestPage::OnCreate()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const USteamAppDefinition* Definition = Sessions ? Sessions->GetDefinition() : nullptr;
	if (!Definition || Definition->Sessions.IsEmpty())
	{
		Status = LOCTEXT("NoProfile", "Needs the Sessions feature and a session profile with a tag in the Steam App Definition (Sessions array).");
		return FReply::Handled();
	}

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->CreateSession(Definition->Sessions[0].Tag, FSteamSessionOpDelegate::CreateLambda([Async](const FSteamResult& Done)
	{
		*Async = Done.IsSuccess() ? LOCTEXT("Created", "Session created.") : FText::Format(LOCTEXT("CreateFailed", "Create failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Creating", "Creating the session...") : FText::Format(LOCTEXT("CreateRefused", "Create refused: {0}"), Result.Message);
	return FReply::Handled();
}

FText SSandwichSteamSessionsTestPage::BuildAppliedSettingsText() const
{
	const USteamSessionsSubsystem* Sessions = GetSessions();
	FSteamSessionSettings Applied;
	FGameplayTag ProfileTag;
	if (!Sessions || !Sessions->GetCurrentSettings(Applied, ProfileTag).IsSuccess())
	{
		return LOCTEXT("NoAppliedSettings", "Applied settings: not hosting or in a lobby.");
	}

	return FText::Format(LOCTEXT("AppliedSettings", "Applied settings: name '{0}', {1} players, {2}, join in progress {3}, presence {4}, {5} setting(s), profile {6}"),
		FText::FromString(Applied.DisplayName), FText::AsNumber(Applied.MaxPlayers),
		FText::FromString(Applied.Visibility == ESteamSessionVisibility::Public ? TEXT("Public") : (Applied.Visibility == ESteamSessionVisibility::FriendsOnly ? TEXT("Friends Only") : TEXT("Private"))),
		SandwichSteamTest::BoolText(Applied.bAllowJoinInProgress), SandwichSteamTest::BoolText(Applied.bUsesPresence), FText::AsNumber(Applied.Settings.Num()),
		FText::FromString(ProfileTag.IsValid() ? ProfileTag.ToString() : TEXT("none")));
}

FReply SSandwichSteamSessionsTestPage::OnCreateWithOptions()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const USteamAppDefinition* Definition = Sessions ? Sessions->GetDefinition() : nullptr;
	if (!Definition || Definition->Sessions.IsEmpty())
	{
		Status = LOCTEXT("NoProfile", "Needs the Sessions feature and a session profile with a tag in the Steam App Definition (Sessions array).");
		return FReply::Handled();
	}

	const FSteamSessionProfileDef& Profile = Definition->Sessions[0];
	FSteamSessionSettings Requested;
	Requested.DisplayName = TEXT("OST test");
	Requested.Visibility = ESteamSessionVisibility::FriendsOnly;
	Requested.MaxPlayers = FMath::Max(Profile.MaxPlayers - 1, 1);

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->CreateSession(Profile.Tag, Requested, FSteamSessionSettingsOpDelegate::CreateLambda([Async](const FSteamResult& Done, const FSteamSessionSettings& Applied)
	{
		*Async = Done.IsSuccess()
			? FText::Format(LOCTEXT("CreatedWithOptions", "Session created. Applied: name '{0}', {1} players, Friends Only."), FText::FromString(Applied.DisplayName), FText::AsNumber(Applied.MaxPlayers))
			: FText::Format(LOCTEXT("CreateFailed", "Create failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Creating", "Creating the session...") : FText::Format(LOCTEXT("CreateRefused", "Create refused: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnCreateBreakingRules()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const USteamAppDefinition* Definition = Sessions ? Sessions->GetDefinition() : nullptr;
	if (!Definition || Definition->Sessions.IsEmpty())
	{
		Status = LOCTEXT("NoProfile", "Needs the Sessions feature and a session profile with a tag in the Steam App Definition (Sessions array).");
		return FReply::Handled();
	}

	// Pick a visibility the first profile's Allowed Visibilities does not permit; Private if every visibility is allowed
	// (an empty mask), since a profile with no restriction at all cannot be broken this way.
	const FSteamSessionProfileDef& Profile = Definition->Sessions[0];
	ESteamSessionVisibility Breaking = ESteamSessionVisibility::Private;
	if (Profile.AllowedVisibilities != 0)
	{
		for (const ESteamSessionVisibility Candidate : { ESteamSessionVisibility::Public, ESteamSessionVisibility::FriendsOnly, ESteamSessionVisibility::Private })
		{
			if ((Profile.AllowedVisibilities & static_cast<uint8>(1 << static_cast<uint8>(Candidate))) == 0)
			{
				Breaking = Candidate;
				break;
			}
		}
	}

	FSteamSessionSettings Requested;
	Requested.Visibility = Breaking;

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->CreateSession(Profile.Tag, Requested, FSteamSessionSettingsOpDelegate::CreateLambda([Async](const FSteamResult& Done, const FSteamSessionSettings&)
	{
		*Async = Done.IsSuccess()
			? LOCTEXT("BrokenRuleNotRefused", "Unexpected: the session was created. The profile may allow every visibility (Allowed Visibilities empty), so this test cannot break it.")
			: FText::Format(LOCTEXT("BrokenRuleRefused", "Refused as expected ({0}): {1}"), FText::FromName(Done.ErrorTag.GetTagName()), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	if (!Result.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("BrokenRuleRefusedSync", "Refused as expected ({0}): {1}"), FText::FromName(Result.ErrorTag.GetTagName()), Result.Message);
	}
	else
	{
		Status = LOCTEXT("CreatingBreaking", "Creating (expected to be refused synchronously; if not, see the async status)...");
	}
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnUpdateToggleVisibility()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		Status = LOCTEXT("NoFeatureUpdate", "The Sessions feature is not available in this world.");
		return FReply::Handled();
	}

	FSteamSessionSettings Current;
	FGameplayTag ProfileTag;
	if (!Sessions->GetCurrentSettings(Current, ProfileTag).IsSuccess())
	{
		Status = LOCTEXT("NoSessionToUpdate", "Create a session or lobby first.");
		return FReply::Handled();
	}

	bToggleToFriendsOnly = !bToggleToFriendsOnly;
	Current.Visibility = bToggleToFriendsOnly ? ESteamSessionVisibility::FriendsOnly : ESteamSessionVisibility::Public;
	Current.DisplayName = TEXT("OST test (updated)");

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->UpdateSession(Current, FSteamSessionSettingsOpDelegate::CreateLambda([Async](const FSteamResult& Done, const FSteamSessionSettings& Applied)
	{
		*Async = Done.IsSuccess()
			? FText::Format(LOCTEXT("Updated", "Updated. Visibility is now {0}."), FText::FromString(Applied.Visibility == ESteamSessionVisibility::Public ? TEXT("Public") : (Applied.Visibility == ESteamSessionVisibility::FriendsOnly ? TEXT("Friends Only") : TEXT("Private"))))
			: FText::Format(LOCTEXT("UpdateFailed", "Update failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Updating", "Updating...") : FText::Format(LOCTEXT("UpdateRefused", "Update refused: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnFind()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		Status = LOCTEXT("NoFeatureFind", "The Sessions feature is not available in this world.");
		return FReply::Handled();
	}

	const TSharedRef<TArray<FSteamSessionResult>> Results = Found;
	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->FindSessions(FSteamSessionSearchOptions(), FSteamSessionFindDelegate::CreateLambda([Results, Async](const FSteamResult& Done, const TArray<FSteamSessionResult>& FoundSessions)
	{
		*Results = FoundSessions;
		*Async = Done.IsSuccess()
			? FText::Format(LOCTEXT("Found", "Search finished: {0} session(s)."), FText::AsNumber(FoundSessions.Num()))
			: FText::Format(LOCTEXT("FindFailed", "Search failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Finding", "Searching...") : FText::Format(LOCTEXT("FindRefused", "Search refused: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnJoinFirst()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	if (!Sessions || Found->IsEmpty())
	{
		Status = LOCTEXT("NothingToJoin", "Find sessions first.");
		return FReply::Handled();
	}

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->JoinSession((*Found)[0], FSteamSessionJoinDelegate::CreateLambda([Async](const FSteamResult& Done, const FString& Connect)
	{
		*Async = Done.IsSuccess()
			? FText::Format(LOCTEXT("Joined", "Joined. Connect string: {0}"), FText::FromString(Connect))
			: FText::Format(LOCTEXT("JoinFailed", "Join failed ({0}): {1}"), FText::FromName(Done.ErrorTag.GetTagName()), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Joining", "Joining...") : FText::Format(LOCTEXT("JoinRefused", "Join refused: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnDestroy()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		Status = LOCTEXT("NoFeatureDestroy", "The Sessions feature is not available in this world.");
		return FReply::Handled();
	}

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->DestroySession(FSteamSessionOpDelegate::CreateLambda([Async](const FSteamResult& Done)
	{
		*Async = Done.IsSuccess() ? LOCTEXT("Destroyed", "Session closed.") : FText::Format(LOCTEXT("DestroyFailed", "Destroy failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Destroying", "Closing the session...") : FText::Format(LOCTEXT("DestroyRefused", "Destroy refused: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnInviteOverlay()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const FSteamResult Result = Sessions ? Sessions->ShowInviteOverlay() : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureInvite", "The Sessions feature is not available in this world."));
	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("InviteOpened", "Invite dialog opened. Invite the other account, accept on the other machine.") : FText::Format(LOCTEXT("InviteFailed", "Invite failed: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnAccept()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const FSteamResult Result = Sessions ? Sessions->AcceptJoinRequest() : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureAccept", "The Sessions feature is not available in this world."));
	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Accepted", "Accepted. Joining...") : FText::Format(LOCTEXT("AcceptFailed", "Accept failed: {0}"), Result.Message);
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnDecline()
{
	if (USteamSessionsSubsystem* Sessions = GetSessions())
	{
		Sessions->DeclineJoinRequest();
	}
	*AsyncStatus = FText::GetEmpty();
	Status = LOCTEXT("Declined", "Waiting request dropped.");
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnHostTravel()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const UWorld* World = WorldContext.IsValid() ? WorldContext->GetWorld() : nullptr;
	if (!Sessions || !World)
	{
		Status = LOCTEXT("NoWorld", "The Sessions feature or the world is not available.");
		return FReply::Handled();
	}

	const FSteamResult Result = Sessions->ServerTravel(World->GetMapName(), /*bListen*/ true);
	*AsyncStatus = FText::GetEmpty();
	Status = Result.IsSuccess() ? LOCTEXT("Travelling", "Reloading this map as a listen server. The other machine can join now.") : FText::Format(LOCTEXT("TravelFailed", "Travel failed: {0}"), Result.Message);
	return FReply::Handled();
}

// ---- Lobby extras (9b) ----

namespace
{
	FText ResultText(const FSteamResult& Result, const FText& OkText)
	{
		return Result.IsSuccess() ? OkText : FText::Format(LOCTEXT("LobbyRefused", "Refused ({0}): {1}"), FText::FromName(Result.ErrorTag.GetTagName()), Result.Message);
	}

	FText NoFeatureText()
	{
		return LOCTEXT("NoFeatureLobby", "The Sessions feature is not available in this world.");
	}
}

FText SSandwichSteamSessionsTestPage::BuildLobbiesText() const
{
	if (FoundLobbies->IsEmpty())
	{
		return LOCTEXT("NoLobbiesFound", "No lobby search results yet (Find lobbies lists lobby only lobbies; the session lobby needs no search).");
	}

	FString Text = FString::Printf(TEXT("Last lobby search: %d lobby(ies)\n"), FoundLobbies->Num());
	for (int32 Index = 0; Index < FoundLobbies->Num(); ++Index)
	{
		const FSteamLobbyInfo& Lobby = (*FoundLobbies)[Index];
		Text += FString::Printf(TEXT("  [%d] lobby %s, owner %s, %d/%d members\n"), Index, *Lobby.LobbyId.ToString(), *Lobby.OwnerId.ToString(), Lobby.MemberCount, Lobby.MaxMembers);
	}
	return FText::FromString(Text.TrimEnd());
}

FReply SSandwichSteamSessionsTestPage::OnLobbyCreate()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	const USteamAppDefinition* Definition = Sessions ? Sessions->GetDefinition() : nullptr;
	if (!Definition || Definition->Sessions.IsEmpty())
	{
		Status = LOCTEXT("NoLobbyProfile", "Needs the Sessions feature and a session profile with a tag in the Steam App Definition (Sessions array).");
		return FReply::Handled();
	}

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->CreateLobby(Definition->Sessions[0].Tag, FSteamLobbyOpDelegate::CreateLambda([Async](const FSteamResult& Done, FSteamId Lobby)
	{
		*Async = Done.IsSuccess() ? FText::Format(LOCTEXT("LobbyCreated", "Lobby {0} created."), FText::FromString(Lobby.ToString())) : FText::Format(LOCTEXT("LobbyCreateFailed", "Lobby create failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = ResultText(Result, LOCTEXT("LobbyCreating", "Creating the lobby..."));
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnLobbyFind()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	if (!Sessions)
	{
		Status = NoFeatureText();
		return FReply::Handled();
	}

	const TSharedRef<TArray<FSteamLobbyInfo>> Results = FoundLobbies;
	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->FindLobbies(FSteamSessionSearchOptions(), FSteamLobbyFindDelegate::CreateLambda([Results, Async](const FSteamResult& Done, const TArray<FSteamLobbyInfo>& Lobbies)
	{
		*Results = Lobbies;
		*Async = Done.IsSuccess() ? FText::Format(LOCTEXT("LobbiesFound", "Lobby search finished: {0} lobby(ies)."), FText::AsNumber(Lobbies.Num())) : FText::Format(LOCTEXT("LobbyFindFailed", "Lobby search failed: {0}"), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = ResultText(Result, LOCTEXT("LobbyFinding", "Searching lobbies..."));
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnLobbyJoinFirst()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	if (!Sessions || FoundLobbies->IsEmpty())
	{
		Status = LOCTEXT("NoLobbyToJoin", "Find lobbies first.");
		return FReply::Handled();
	}

	const TSharedRef<FText> Async = AsyncStatus;
	const FSteamResult Result = Sessions->JoinLobby((*FoundLobbies)[0].LobbyId, FSteamLobbyOpDelegate::CreateLambda([Async](const FSteamResult& Done, FSteamId Lobby)
	{
		*Async = Done.IsSuccess() ? FText::Format(LOCTEXT("LobbyJoined", "Entered lobby {0}."), FText::FromString(Lobby.ToString())) : FText::Format(LOCTEXT("LobbyJoinFailed", "Lobby join failed ({0}): {1}"), FText::FromName(Done.ErrorTag.GetTagName()), Done.Message);
	}));

	*AsyncStatus = FText::GetEmpty();
	Status = ResultText(Result, LOCTEXT("LobbyJoining", "Joining the lobby..."));
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnLobbyLeave()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	*AsyncStatus = FText::GetEmpty();
	Status = Sessions ? ResultText(Sessions->LeaveLobby(), LOCTEXT("LobbyLeft", "Left the lobby only lobby (a session lobby is left with Destroy).")) : NoFeatureText();
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnToggleReady()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	*AsyncStatus = FText::GetEmpty();
	if (!Sessions)
	{
		Status = NoFeatureText();
		return FReply::Handled();
	}

	bReadyState = !bReadyState;
	Status = ResultText(Sessions->SetReady(bReadyState), bReadyState ? LOCTEXT("NowReady", "You are ready.") : LOCTEXT("NowNotReady", "You are not ready."));
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnSendChat()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	*AsyncStatus = FText::GetEmpty();
	Status = Sessions ? ResultText(Sessions->SendChatMessage(FString::Printf(TEXT("test message %d"), ++ChatCounter)), LOCTEXT("ChatSent", "Chat sent. The message shows in the log of every member (LogSandwichSteam: lobby chat from ...).")) : NoFeatureText();
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnSetLobbyData()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	*AsyncStatus = FText::GetEmpty();
	Status = Sessions ? ResultText(Sessions->SetLobbyData(TEXT("testkey"), FDateTime::Now().ToString()), LOCTEXT("DataSet", "Lobby data 'testkey' set. Other members read it with Steam.Sessions.LobbyData testkey.")) : NoFeatureText();
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnKickOther()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	*AsyncStatus = FText::GetEmpty();
	if (!Sessions)
	{
		Status = NoFeatureText();
		return FReply::Handled();
	}

	for (const FSteamLobbyMember& Member : Sessions->GetLobbyMembers())
	{
		if (!Member.bIsLocal)
		{
			Status = ResultText(Sessions->KickMember(Member.Id, TEXT("test kick")), FText::Format(LOCTEXT("Kicked", "Kick marker written for {0}. That player leaves the lobby."), FText::FromString(Member.Name)));
			return FReply::Handled();
		}
	}

	Status = LOCTEXT("NobodyToKick", "There is no other member to kick.");
	return FReply::Handled();
}

FReply SSandwichSteamSessionsTestPage::OnForgiveAll()
{
	USteamSessionsSubsystem* Sessions = GetSessions();
	*AsyncStatus = FText::GetEmpty();
	if (!Sessions)
	{
		Status = NoFeatureText();
		return FReply::Handled();
	}

	// A kicked player is no longer a member, so the markers are found by their keys.
	int32 Removed = 0;
	FSteamResult Last = FSteamResult::Success();
	for (const TPair<FString, FString>& Pair : Sessions->GetAllLobbyData())
	{
		FSteamId Member;
		if (Pair.Key.StartsWith(TEXT("kick_")) && FSteamId::FromString(Pair.Key.RightChop(5), Member))
		{
			Last = Sessions->ForgiveMember(Member);
			if (!Last.IsSuccess())
			{
				break;
			}
			++Removed;
		}
	}

	Status = ResultText(Last, FText::Format(LOCTEXT("Forgiven", "{0} kick marker(s) removed."), FText::AsNumber(Removed)));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
