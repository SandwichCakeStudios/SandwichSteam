// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Style/SteamEditorWidgets.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Style/SteamToolStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamEditorWidgets"

namespace SandwichSteam::Editor
{
	FSlateFontInfo BodyFont()
	{
		return FSteamToolStyle::ScaleFont(FCoreStyle::GetDefaultFontStyle("Regular", 9), 1.25f);
	}

	void Notify(const FText& Text, SNotificationItem::ECompletionState State, float Duration)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = Duration;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	void NotifyFileWritten(const FText& Text, const FString& FilePath, bool bSuccess)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 12.0f;
		if (bSuccess && !FilePath.IsEmpty())
		{
			Info.HyperlinkText = LOCTEXT("ShowFile", "Show file");
			Info.Hyperlink = FSimpleDelegate::CreateLambda([FilePath]()
			{
				FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FilePath));
			});
		}

		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, TFunction<void()> OnClick, TAttribute<bool> bEnabled)
	{
		return SNew(SButton)
			.ToolTipText(ToolTip)
			.IsEnabled(bEnabled)
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label).Font(BodyFont())
			];
	}
}

#undef LOCTEXT_NAMESPACE
