// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Features/User/SteamUserTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class USteamUserSubsystem;

/** Test page for the User feature: persona, ownership, avatars (any user, any size) and the Web API ticket. */
class SSandwichSteamUserTestPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSandwichSteamUserTestPage) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UObject>, WorldContext)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	USteamUserSubsystem* GetUser() const;

	FReply OnLoadAvatar(ESteamAvatarSize Size);
	FReply OnRequestTicket();
	void HandleAvatar(const FSteamResult& Result, UTexture2D* Texture);
	void HandleTicket(const FSteamResult& Result, const FString& TicketHex, int32 TicketHandle);

	TWeakObjectPtr<UObject> WorldContext;
	TSharedPtr<SEditableTextBox> UserIdBox;
	TSharedPtr<SEditableTextBox> IdentityBox;

	FSlateBrush AvatarBrush;
	TStrongObjectPtr<UTexture2D> AvatarTexture;
	FText Status;
};
