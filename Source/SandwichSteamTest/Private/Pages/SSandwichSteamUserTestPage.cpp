// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Pages/SSandwichSteamUserTestPage.h"
#include "Core/SteamId.h"
#include "Engine/Texture2D.h"
#include "Features/User/SteamUserSubsystem.h"
#include "Pages/SandwichSteamTestSlate.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SandwichSteamTestUser"

void SSandwichSteamUserTestPage::Construct(const FArguments& InArgs)
{
	WorldContext = InArgs._WorldContext;
	AvatarBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	Status = LOCTEXT("Ready", "Ready.");

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Local", "Local user")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Active", "Feature active"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamUserSubsystem* User = GetUser();
				return SandwichSteamTest::BoolText(User && User->IsFeatureActive());
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Persona", "Persona name"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamUserSubsystem* User = GetUser();
				return User ? FText::FromString(User->GetPersonaName()) : FText::GetEmpty();
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("SteamId", "Steam ID"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamUserSubsystem* User = GetUser();
				return User ? FText::FromString(User->GetLocalSteamId().ToString()) : FText::GetEmpty();
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Level", "Steam level / logged on"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamUserSubsystem* User = GetUser();
				return User ? FText::Format(LOCTEXT("LevelFmt", "{0} / {1}"), User->GetSteamLevel(), SandwichSteamTest::BoolText(User->IsLoggedOn())) : FText::GetEmpty();
			}))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SandwichSteamTest::MakeRow(LOCTEXT("Owns", "Owns game / Family Sharing"), TAttribute<FText>::CreateLambda([this]
			{
				const USteamUserSubsystem* User = GetUser();
				return User ? FText::Format(LOCTEXT("OwnsFmt", "{0} / {1}"), SandwichSteamTest::BoolText(User->IsSubscribed()), SandwichSteamTest::BoolText(User->IsFamilySharedLicense())) : FText::GetEmpty();
			}))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Avatar", "Avatar")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Padding(4.f)
				[
					SNew(SBox).WidthOverride(184.f).HeightOverride(184.f)
					[
						SNew(SImage).Image(&AvatarBrush)
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
				[
					SAssignNew(UserIdBox, SEditableTextBox)
					.HintText(LOCTEXT("IdHint", "Steam ID (empty = you). Try a friend's or a non-friend's ID"))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SWrapBox).UseAllottedSize(true)
					+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Small", "Small"), FOnClicked::CreateSP(this, &SSandwichSteamUserTestPage::OnLoadAvatar, ESteamAvatarSize::Small)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Medium", "Medium"), FOnClicked::CreateSP(this, &SSandwichSteamUserTestPage::OnLoadAvatar, ESteamAvatarSize::Medium)) ]
					+ SWrapBox::Slot().Padding(0.f, 0.f, 8.f, 6.f)[ SandwichSteamTest::MakeButton(LOCTEXT("Large", "Large"), FOnClicked::CreateSP(this, &SSandwichSteamUserTestPage::OnLoadAvatar, ESteamAvatarSize::Large)) ]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 6.f)[ SandwichSteamTest::MakeSectionTitle(LOCTEXT("Ticket", "Web API ticket")) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SAssignNew(IdentityBox, SEditableTextBox)
				.Text(LOCTEXT("DefaultIdentity", "SandwichSteamTest"))
				.HintText(LOCTEXT("IdentityHint", "Service identity"))
			]
			+ SHorizontalBox::Slot().AutoWidth()[ SandwichSteamTest::MakeButton(LOCTEXT("GetTicket", "Get ticket"), FOnClicked::CreateSP(this, &SSandwichSteamUserTestPage::OnRequestTicket)) ]
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

USteamUserSubsystem* SSandwichSteamUserTestPage::GetUser() const
{
	return USteamUserSubsystem::Get(WorldContext.Get());
}

FReply SSandwichSteamUserTestPage::OnLoadAvatar(ESteamAvatarSize Size)
{
	USteamUserSubsystem* User = GetUser();
	if (!User)
	{
		Status = LOCTEXT("NoUser", "User feature is not available.");
		return FReply::Handled();
	}

	FSteamId Target = User->GetLocalSteamId();
	const FString Text = UserIdBox->GetText().ToString().TrimStartAndEnd();
	if (!Text.IsEmpty() && !FSteamId::FromString(Text, Target))
	{
		Status = LOCTEXT("BadId", "That is not a valid Steam ID (64-bit decimal or [U:1:n]).");
		return FReply::Handled();
	}

	Status = LOCTEXT("Loading", "Requesting avatar...");
	const FSteamResult Result = User->RequestAvatar(Target, Size, FSteamAvatarResultDelegate::CreateSP(this, &SSandwichSteamUserTestPage::HandleAvatar));
	if (!Result.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("AvatarFailed", "Avatar request failed: {0}"), Result.Message);
	}
	return FReply::Handled();
}

void SSandwichSteamUserTestPage::HandleAvatar(const FSteamResult& Result, UTexture2D* Texture)
{
	if (!Result.IsSuccess() || !Texture)
	{
		AvatarBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		AvatarTexture.Reset();
		Status = FText::Format(LOCTEXT("AvatarError", "Avatar failed: {0}"), Result.Message);
		return;
	}

	AvatarTexture.Reset(Texture); // Keeps the texture alive even if the cache evicts it.
	AvatarBrush.SetResourceObject(Texture);
	AvatarBrush.ImageSize = FVector2f(Texture->GetSizeX(), Texture->GetSizeY());
	AvatarBrush.DrawAs = ESlateBrushDrawType::Image;
	Status = FText::Format(LOCTEXT("AvatarOk", "Avatar loaded: {0}x{1}"), Texture->GetSizeX(), Texture->GetSizeY());
}

FReply SSandwichSteamUserTestPage::OnRequestTicket()
{
	USteamUserSubsystem* User = GetUser();
	if (!User)
	{
		Status = LOCTEXT("NoUser2", "User feature is not available.");
		return FReply::Handled();
	}

	Status = LOCTEXT("TicketWait", "Requesting Web API ticket...");
	const FSteamResult Result = User->RequestWebApiTicket(IdentityBox->GetText().ToString(), FSteamWebApiTicketDelegate::CreateSP(this, &SSandwichSteamUserTestPage::HandleTicket));
	if (!Result.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("TicketFailed", "Ticket request failed: {0}"), Result.Message);
	}
	return FReply::Handled();
}

void SSandwichSteamUserTestPage::HandleTicket(const FSteamResult& Result, const FString& TicketHex, int32 TicketHandle)
{
	if (!Result.IsSuccess())
	{
		Status = FText::Format(LOCTEXT("TicketError", "Ticket failed: {0}"), Result.Message);
		return;
	}

	Status = FText::Format(LOCTEXT("TicketOk", "Ticket ok: {0} bytes, handle {1}, starts with {2}... (cancelled again)"),
		TicketHex.Len() / 2, TicketHandle, FText::FromString(TicketHex.Left(24)));

	if (USteamUserSubsystem* User = GetUser())
	{
		User->CancelWebApiTicket(TicketHandle);
	}
}

#undef LOCTEXT_NAMESPACE
