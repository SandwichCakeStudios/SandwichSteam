// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamFriendsTestPage.h"
#include "HAL/PlatformTime.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "SteamFriendsSubsystem.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestFriends"

void SSandwichSteamFriendsTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	Status = LOCTEXT("Ready", "Ready. Every read shows the first entries and how long it took.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Friends", "Friends")) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamFriendsSubsystem* Friends = GetFriends();
			return Friends ? SandwichSteamTest::BoolText(Friends->IsFeatureActive()) : LOCTEXT("NotAvailable", "not available");
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Count", "Friends"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamFriendsSubsystem* Friends = GetFriends();
			return Friends ? FText::AsNumber(Friends->GetFriendCount()) : FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight()[ SandwichSteamTest::MakeRow(LOCTEXT("Changes", "Persona changes / events"), TAttribute<FText>::CreateLambda([this]
		{
			const USteamFriendsSubsystem* Friends = GetFriends();
			return Friends
				? FText::Format(LOCTEXT("ChangesFmt", "{0} / {1}"), FText::AsNumber(Friends->GetPersonaChangeCount()), FText::AsNumber(Friends->GetBroadcastCount()))
				: FText::GetEmpty();
		})) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(STextBlock).AutoWrapText(true).Text_Lambda([this] { return BuildListText(); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("All", "All friends"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnRead, ESteamFriendSource::Friends, ESteamFriendFilter::All)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Online", "Online"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnRead, ESteamFriendSource::Friends, ESteamFriendFilter::Online)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("InThisGame", "In this game"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnRead, ESteamFriendSource::Friends, ESteamFriendFilter::InThisGame)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Recent", "Recent players"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnRead, ESteamFriendSource::RecentPlayers, ESteamFriendFilter::All)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Blocked", "Blocked"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnRead, ESteamFriendSource::Blocked, ESteamFriendFilter::All)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Groups", "Friend groups"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnShowGroups)) ]
			+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Overlay", "Open friends list"), FOnClicked::CreateSP(this, &SSandwichSteamFriendsTestPage::OnOpenFriendsList)) ]
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

USteamFriendsSubsystem* SSandwichSteamFriendsTestPage::GetFriends() const
{
	return USteamFriendsSubsystem::Get(WorldContext.Get());
}

FText SSandwichSteamFriendsTestPage::BuildListText() const
{
	if (List.IsEmpty())
	{
		return LOCTEXT("NoEntries", "No users loaded yet.");
	}

	constexpr int32 MaxShown = 20;
	FString Text;
	for (int32 Index = 0; Index < FMath::Min(List.Num(), MaxShown); ++Index)
	{
		const FSteamFriendInfo& Info = List[Index];
		Text += FString::Printf(TEXT("%s  (state %d)%s%s\n"), *Info.Name, static_cast<int32>(Info.PersonaState),
			Info.bInThisGame ? TEXT("  playing this game") : (Info.bInGame ? *FString::Printf(TEXT("  playing %d"), Info.GameAppId) : TEXT("")),
			Info.bInfoLoaded ? TEXT("") : TEXT("  (data pending)"));
	}
	if (List.Num() > MaxShown)
	{
		Text += FString::Printf(TEXT("... and %d more\n"), List.Num() - MaxShown);
	}
	return FText::FromString(Text.TrimEnd());
}

FReply SSandwichSteamFriendsTestPage::OnRead(ESteamFriendSource Source, ESteamFriendFilter Filter)
{
	USteamFriendsSubsystem* Friends = GetFriends();
	if (!Friends)
	{
		Status = LOCTEXT("NoFeature", "The Friends feature is not available in this world.");
		return FReply::Handled();
	}

	const double Start = FPlatformTime::Seconds();
	const FSteamResult Started = Friends->ReadFriends(Source, Filter, FSteamReadFriendsDelegate::CreateSP(this, &SSandwichSteamFriendsTestPage::HandleRead, Start));
	if (!Started.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("ReadFailed", "Read failed: {0}"), Started.Message);
	}
	return FReply::Handled();
}

void SSandwichSteamFriendsTestPage::HandleRead(const FSteamResult& Result, const TArray<FSteamFriendInfo>& NewList, double StartSeconds)
{
	if (!Result.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("ReadError", "Read failed: {0}"), Result.Message);
		return;
	}

	List = NewList;
	Status = FText::Format(LOCTEXT("Read", "{0} user(s) in {1} ms."), FText::AsNumber(List.Num()), FText::AsNumber(FMath::RoundToInt((FPlatformTime::Seconds() - StartSeconds) * 1000.0)));
}

FReply SSandwichSteamFriendsTestPage::OnShowGroups()
{
	const USteamFriendsSubsystem* Friends = GetFriends();
	if (!Friends)
	{
		Status = LOCTEXT("NoFeatureGroups", "The Friends feature is not available in this world.");
		return FReply::Handled();
	}

	const TArray<FSteamFriendGroup> Groups = Friends->GetFriendGroups();
	FString Text = FString::Printf(TEXT("%d friend group(s)."), Groups.Num());
	for (const FSteamFriendGroup& Group : Groups)
	{
		Text += FString::Printf(TEXT(" %s (%d)"), *Group.Name, Group.Members.Num());
	}
	Status = FText::FromString(Text);
	return FReply::Handled();
}

FReply SSandwichSteamFriendsTestPage::OnOpenFriendsList()
{
	USteamFriendsSubsystem* Friends = GetFriends();
	const FSteamResult Result = Friends ? Friends->OpenFriendsList() : FSteamResult::Failure(FGameplayTag(), LOCTEXT("NoFeatureOverlay", "The Friends feature is not available in this world."));
	Status = Result.IsSuccess() ? LOCTEXT("Opened", "Friends list opened.") : FText::Format(LOCTEXT("OpenFailed", "Could not open it: {0}"), Result.Message);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
