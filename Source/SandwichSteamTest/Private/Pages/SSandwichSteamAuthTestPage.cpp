// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamAuthTestPage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/DateTime.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamAuthLibrary.h"
#include "SteamAuthSubsystem.h"
#include "SteamAuthTypes.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestAuth"

SSandwichSteamAuthTestPage::~SSandwichSteamAuthTestPage()
{
	Unbind();
	if (Listener.IsValid())
	{
		Listener->OnResult = nullptr;
		Listener->OnFailed = nullptr;
	}
}

void SSandwichSteamAuthTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Needs Enable Steam Authentication (Project Settings > Plugins > Sandwich Steam - Auth), Configure Steam and a restart, on host and client. Host here (Sessions tab: create, host travel), join from a second machine with a second Steam account, then watch the events. Set the failure policy to Custom to test Kick.");

	Listener.Reset(NewObject<USandwichSteamAuthTestListener>(GetTransientPackage()));
	Listener->OnResult = [this](const FSteamAuthResult& Result) { HandleResult(Result); };
	Listener->OnFailed = [this](FSteamId Player) { HandleFailed(Player); };
	UpdateBinding();

	const TSharedRef<SVerticalBox> PlayerRows = SNew(SVerticalBox);
	for (int32 Index = 0; Index < MaxRows; ++Index)
	{
		PlayerRows->AddSlot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this, Index] { return Rows.IsValidIndex(Index) ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(STextBlock).Text_Lambda([this, Index] { return BuildRowText(Index); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Visibility_Lambda([this, Index] { return (Rows.IsValidIndex(Index) && !Rows[Index].bLocal) ? EVisibility::Visible : EVisibility::Hidden; })
				[
					SandwichSteamTest::MakeButton(LOCTEXT("Kick", "Kick"), FOnClicked::CreateSP(this, &SSandwichSteamAuthTestPage::OnKick, Index))
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Auth", "Steam authentication")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamAuthSubsystem* Auth = GetAuth();
			return Auth ? SandwichSteamTest::BoolText(USteamAuthLibrary::IsSteamAuthActive(WorldContext.Get())) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("PacketHandler", "SteamAuth packet handler"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamAuthSubsystem* Auth = GetAuth();
			return Auth ? SandwichSteamTest::BoolText(Auth->IsPacketHandlerEnabled()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Policy", "Failure policy"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamAuthSubsystem* Auth = GetAuth();
			if (!Auth || !Auth->IsFeatureActive())
			{
				return FText::GetEmpty();
			}
			return Auth->GetFailurePolicy() == ESteamAuthFailurePolicy::Custom
				? LOCTEXT("Custom", "Custom (nobody is kicked automatically, On Steam Auth Failed decides)")
				: LOCTEXT("KickPolicy", "Kick (the engine kicks failed players)");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Role", "This machine"), TAttribute<FText>::CreateSP(this, &SSandwichSteamAuthTestPage::BuildRoleText)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Players", "Players in the game")) ]
		+ SVerticalBox::Slot().AutoHeight()[ PlayerRows ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Events", "Events (newest first)")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildEventsText(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildStateText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("ClearEvents", "Clear events"), FOnClicked::CreateSP(this, &SSandwichSteamAuthTestPage::OnClearEvents)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)))
			.Text_Lambda([this] { return Status; })
		]
	];
}

USteamAuthSubsystem* SSandwichSteamAuthTestPage::GetAuth() const
{
	return USteamAuthSubsystem::Get(WorldContext.Get());
}

void SSandwichSteamAuthTestPage::UpdateBinding()
{
	USteamAuthSubsystem* Auth = GetAuth();
	if (Auth == BoundAuth.Get() || !Listener.IsValid())
	{
		return;
	}

	Unbind();
	if (Auth)
	{
		Auth->OnSteamAuthResult.AddUniqueDynamic(Listener.Get(), &USandwichSteamAuthTestListener::HandleResult);
		Auth->OnSteamAuthFailed.AddUniqueDynamic(Listener.Get(), &USandwichSteamAuthTestListener::HandleFailed);
		BoundAuth = Auth;
	}
}

void SSandwichSteamAuthTestPage::Unbind()
{
	USteamAuthSubsystem* Auth = BoundAuth.Get();
	if (Auth && Listener.IsValid())
	{
		Auth->OnSteamAuthResult.RemoveDynamic(Listener.Get(), &USandwichSteamAuthTestListener::HandleResult);
		Auth->OnSteamAuthFailed.RemoveDynamic(Listener.Get(), &USandwichSteamAuthTestListener::HandleFailed);
	}
	BoundAuth.Reset();
}

void SSandwichSteamAuthTestPage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UpdateBinding();

	// The game state's player list works on host and client; only the host has auth results for remote players.
	Rows.Reset();
	const UWorld* World = (GEngine && WorldContext.IsValid()) ? GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (Rows.Num() >= MaxRows)
		{
			break;
		}
		if (!PlayerState)
		{
			continue;
		}

		FPlayerRow& Row = Rows.AddDefaulted_GetRef();
		Row.PlayerState = PlayerState;
		if (PlayerState->GetUniqueId().IsValid())
		{
			FSteamId::FromUniqueNetId(*PlayerState->GetUniqueId(), Row.Player);
		}
		const APlayerController* Controller = Cast<APlayerController>(PlayerState->GetOwningController());
		Row.bLocal = Controller && Controller->IsLocalController();
	}
}

void SSandwichSteamAuthTestPage::HandleResult(const FSteamAuthResult& Result)
{
	AddEvent(FString::Printf(TEXT("On Steam Auth Result: %s %s (%s)"), *Result.Player.ToString(),
		Result.bSuccess ? TEXT("PASSED") : TEXT("FAILED"), *UEnum::GetDisplayValueAsText(Result.Response).ToString()));
}

void SSandwichSteamAuthTestPage::HandleFailed(FSteamId Player)
{
	AddEvent(FString::Printf(TEXT("On Steam Auth Failed: %s (Custom policy, not kicked; use Kick on the player row)"), *Player.ToString()));
}

void SSandwichSteamAuthTestPage::AddEvent(const FString& Line)
{
	Events.Insert(FString::Printf(TEXT("%s  %s"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Line), 0);
	if (Events.Num() > MaxEvents)
	{
		Events.SetNum(MaxEvents);
	}
}

FText SSandwichSteamAuthTestPage::BuildRoleText() const
{
	const UWorld* World = (GEngine && WorldContext.IsValid()) ? GEngine->GetWorldFromContextObject(WorldContext.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return FText::GetEmpty();
	}

	switch (World->GetNetMode())
	{
	case NM_ListenServer:    return LOCTEXT("Listen", "listen server host (results arrive here)");
	case NM_DedicatedServer: return LOCTEXT("Dedicated", "dedicated server (not supported by the plugin)");
	case NM_Client:          return LOCTEXT("Client", "client (no results here: Steam checks you on the host)");
	default:                 return LOCTEXT("Standalone", "standalone (nobody can join: host a session first)");
	}
}

FText SSandwichSteamAuthTestPage::BuildRowText(int32 Index) const
{
	if (!Rows.IsValidIndex(Index))
	{
		return FText::GetEmpty();
	}

	const FPlayerRow& Row = Rows[Index];
	const APlayerState* PlayerState = Row.PlayerState.Get();
	const FString Name = PlayerState ? PlayerState->GetPlayerName() : FString();
	const FString Id = Row.Player.IsValid() ? Row.Player.ToString() : FString(TEXT("no Steam ID"));
	const bool bAuthenticated = USteamAuthLibrary::IsPlayerSteamAuthenticated(PlayerState);

	if (Row.bLocal)
	{
		return FText::FromString(FString::Printf(TEXT("%s  %s  (this machine, never checked)  authenticated: %s"), *Id, *Name, bAuthenticated ? TEXT("yes") : TEXT("no")));
	}

	const ESteamAuthStatus AuthStatus = USteamAuthLibrary::GetSteamAuthStatus(WorldContext.Get(), Row.Player);
	FSteamAuthResult Result;
	const FString Response = USteamAuthLibrary::GetSteamAuthResult(WorldContext.Get(), Row.Player, Result)
		? UEnum::GetDisplayValueAsText(Result.Response).ToString()
		: FString(TEXT("no result"));

	return FText::FromString(FString::Printf(TEXT("%s  %s  %s  (%s)  authenticated: %s"), *Id, *Name,
		*UEnum::GetDisplayValueAsText(AuthStatus).ToString(), *Response, bAuthenticated ? TEXT("yes") : TEXT("no")));
}

FText SSandwichSteamAuthTestPage::BuildEventsText() const
{
	if (Events.IsEmpty())
	{
		return LOCTEXT("NoEvents", "No events yet. They fire on the host shortly after a player's PostLogin.");
	}
	return FText::FromString(FString::Join(Events, TEXT("\n")));
}

FText SSandwichSteamAuthTestPage::BuildStateText() const
{
#if SANDWICHSTEAM_WITH_DEBUG
	const USteamAuthSubsystem* Auth = GetAuth();
	return Auth ? FText::FromString(Auth->BuildDebugString()) : FText::GetEmpty();
#else
	return FText::GetEmpty();
#endif
}

FReply SSandwichSteamAuthTestPage::OnKick(int32 Index)
{
	if (!Rows.IsValidIndex(Index) || Rows[Index].bLocal)
	{
		return FReply::Handled();
	}

	const FSteamId Player = Rows[Index].Player;
	const FSteamResult Result = USteamAuthLibrary::KickSteamPlayer(WorldContext.Get(), Player, LOCTEXT("KickReason", "Kicked from the Sandwich Steam test panel."));
	Status = Result.IsSuccess()
		? FText::Format(LOCTEXT("Kicked", "Kicked {0}."), FText::FromString(Player.ToString()))
		: FText::Format(LOCTEXT("KickFailed", "Kick failed ({0}): {1}"), FText::FromName(Result.ErrorTag.GetTagName()), Result.Message);
	AddEvent(FString::Printf(TEXT("Kick Steam Player %s: %s"), *Player.ToString(), Result.IsSuccess() ? TEXT("done") : *Result.Message.ToString()));
	return FReply::Handled();
}

FReply SSandwichSteamAuthTestPage::OnClearEvents()
{
	Events.Reset();
	Status = LOCTEXT("Cleared", "Events cleared.");
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
