// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Publish/SteamPublishActions.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "SandwichSteamEditor.h"
#include "Publish/SteamCmdRunner.h"
#include "Publish/SteamPublishSettings.h"
#include "Publish/SteamPublishVdf.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SandwichSteamPublishActions"

namespace
{
	void Notify(const FText& Text, SNotificationItem::ECompletionState State, float Duration = 6.f)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = Duration;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(State);
		}
	}

	/** Absolute steamcmd path and username, or false with a notification. */
	bool GetSteamCmdSetup(FString& OutExe, FString& OutUser)
	{
		const USteamPublishUserSettings* User = USteamPublishUserSettings::Get();
		OutExe = User->SteamCmdPath.FilePath;
		OutUser = User->SteamUsername.TrimStartAndEnd();

		if (OutExe.IsEmpty() || !IFileManager::Get().FileExists(*OutExe))
		{
			Notify(LOCTEXT("NoSteamCmd", "SteamCMD was not found. Set its path in Project Settings > Sandwich Steam - Publish (User)."), SNotificationItem::CS_Fail);
			return false;
		}
		if (!FSteamCmdCommandLine::IsValidUsername(OutUser))
		{
			Notify(LOCTEXT("BadUser", "Enter your Steam account name in Project Settings > Sandwich Steam - Publish (User). Only letters, digits and _ . - @ are allowed."), SNotificationItem::CS_Fail);
			return false;
		}
		return true;
	}

	/** Modal box for the Steam Guard code. Empty optional = cancelled. */
	TOptional<FString> PromptGuardCode(bool bMobile)
	{
		const FText Prompt = bMobile
			? LOCTEXT("GuardMobile", "Enter the code shown in your Steam Mobile authenticator.")
			: LOCTEXT("GuardEmail", "Steam sent a code to your e-mail address. Enter it here.");

		TSharedPtr<SEditableTextBox> CodeBox;
		bool bAccepted = false;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("GuardTitle", "Steam Guard"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		auto Accept = [&Window, &bAccepted]()
		{
			bAccepted = true;
			Window->RequestDestroyWindow();
			return FReply::Handled();
		};

		Window->SetContent(
			SNew(SBox).MinDesiredWidth(360.f).Padding(16.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
				[
					SNew(STextBlock).Text(Prompt).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
				[
					SAssignNew(CodeBox, SEditableTextBox)
					.OnTextCommitted_Lambda([&Accept](const FText&, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::OnEnter)
						{
							Accept();
						}
					})
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(SButton).Text(LOCTEXT("GuardOk", "Send")).OnClicked_Lambda(Accept)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(LOCTEXT("GuardCancel", "Cancel")).OnClicked_Lambda([&Window]()
						{
							Window->RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]);

		GEditor->EditorAddModalWindow(Window);

		const FString Code = CodeBox.IsValid() ? CodeBox->GetText().ToString().TrimStartAndEnd() : FString();
		if (bAccepted && !Code.IsEmpty())
		{
			return Code;
		}
		return TOptional<FString>();
	}

	TSharedPtr<FSteamCmdRunner> LoginTestRunner;
}

namespace SandwichSteam::Editor
{
	TOptional<FString> PromptSteamGuardCode(bool bMobile)
	{
		return PromptGuardCode(bMobile);
	}

	void GenerateVdfDryRun()
	{
		const USteamPublishSettings* Settings = USteamPublishSettings::Get();
		const FString Branch = Settings->Branches.IsEmpty() ? FString() : Settings->Branches[0].Name;

		SandwichSteam::Publish::FVdfFiles Files;
		FString Error;
		if (!SandwichSteam::Publish::WriteVdfFiles(*Settings, Branch, Files, Error))
		{
			UE_LOG(LogSandwichSteamEditor, Warning, TEXT("Publish dry run failed: %s"), *Error);
			Notify(FText::FromString(Error), SNotificationItem::CS_Fail, 10.f);
			return;
		}

		for (const FString& Warning : Files.Warnings)
		{
			UE_LOG(LogSandwichSteamEditor, Warning, TEXT("Publish dry run: %s"), *Warning);
		}
		UE_LOG(LogSandwichSteamEditor, Log, TEXT("Publish dry run wrote %s (%d depot script(s))"), *Files.AppVdfPath, Files.DepotVdfPaths.Num());
		Notify(FText::Format(LOCTEXT("DryRunDone", "Wrote {0} and {1} depot script(s). Nothing was uploaded."), FText::FromString(Files.AppVdfPath), Files.DepotVdfPaths.Num()), SNotificationItem::CS_Success, 10.f);
	}

	void SandwichSteamCmdLoginTerminal()
	{
		FString Exe, User;
		if (!GetSteamCmdSetup(Exe, User))
		{
			return;
		}

		const FString SteamCmdArgs = FSteamCmdCommandLine::BuildTerminalLogin(User);
#if PLATFORM_WINDOWS
		FString WindowsExe = Exe;
		WindowsExe.ReplaceInline(TEXT("/"), TEXT("\\"));
		// "start" gives the console its own window; cmd /k keeps it open after SteamCMD ends.
		const FString Params = FString::Printf(TEXT("/c start \"SteamCMD login\" cmd /k \"\"%s\" %s\""), *WindowsExe, *SteamCmdArgs);
		FPlatformProcess::CreateProc(TEXT("cmd.exe"), *Params, false, true, true, nullptr, 0, nullptr, nullptr);
		Notify(LOCTEXT("TerminalOpened", "A terminal opened. Enter your password and Steam Guard code there, wait for \"OK\", then type quit. Then use Test login."), SNotificationItem::CS_Success, 12.f);
#else
		const FString Command = FString::Printf(TEXT("\"%s\" %s"), *Exe, *SteamCmdArgs);
		FPlatformApplicationMisc::ClipboardCopy(*Command);
		Notify(LOCTEXT("TerminalCopied", "Command copied to the clipboard. Run it in a terminal, enter password and Steam Guard code, wait for \"OK\", then type quit."), SNotificationItem::CS_Success, 12.f);
#endif
	}

	void TestSteamCmdLogin()
	{
		if (LoginTestRunner.IsValid() && LoginTestRunner->IsRunning())
		{
			Notify(LOCTEXT("TestRunning", "A login test is already running."), SNotificationItem::CS_Fail);
			return;
		}

		FString Exe, User;
		if (!GetSteamCmdSetup(Exe, User))
		{
			return;
		}

		LoginTestRunner = MakeShared<FSteamCmdRunner>();
		const TWeakPtr<FSteamCmdRunner> WeakRunner = LoginTestRunner;

		LoginTestRunner->OnEvent().AddLambda([WeakRunner](const FSteamCmdEvent& Event)
		{
			if (Event.Type == ESteamCmdEventType::Line)
			{
				UE_LOG(LogSandwichSteamEditor, Log, TEXT("steamcmd: %s"), *Event.Text);
			}
			else if (Event.Type == ESteamCmdEventType::GuardEmailRequested || Event.Type == ESteamCmdEventType::GuardMobileRequested)
			{
				const bool bMobile = Event.Type == ESteamCmdEventType::GuardMobileRequested;
				// Next tick: the dialog is modal and must not run inside the event delivery.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakRunner, bMobile](float)
				{
					if (const TSharedPtr<FSteamCmdRunner> Runner = WeakRunner.Pin())
					{
						const TOptional<FString> Code = PromptGuardCode(bMobile);
						if (Code.IsSet())
						{
							Runner->SubmitGuardCode(Code.GetValue());
						}
						else
						{
							Runner->Cancel();
						}
					}
					return false;
				}));
			}
		});

		LoginTestRunner->OnFinished().AddLambda([](const FSteamCmdResult& Result)
		{
			switch (Result.Outcome)
			{
			case ESteamCmdOutcome::Success:
				Notify(Result.bLoggedIn ? LOCTEXT("TestOk", "SteamCMD login works.") : LOCTEXT("TestOkNoLine", "SteamCMD finished without errors, but no login line was seen. Check the log."), SNotificationItem::CS_Success);
				break;
			case ESteamCmdOutcome::NeedsPassword:
				Notify(LOCTEXT("TestPassword", "SteamCMD has no cached login. Use \"Open login terminal\" and log in once."), SNotificationItem::CS_Fail, 10.f);
				break;
			case ESteamCmdOutcome::LoginFailed:
				Notify(FText::Format(LOCTEXT("TestLoginFailed", "Steam login failed: {0}"), FText::FromString(Result.Message)), SNotificationItem::CS_Fail, 10.f);
				break;
			case ESteamCmdOutcome::Canceled:
				Notify(LOCTEXT("TestCanceled", "Login test cancelled."), SNotificationItem::CS_Fail);
				break;
			default:
				Notify(FText::Format(LOCTEXT("TestFailed", "SteamCMD failed (exit code {0}). {1}"), Result.ReturnCode, FText::FromString(Result.Message)), SNotificationItem::CS_Fail, 10.f);
				break;
			}
		});

		FString Error;
		if (!LoginTestRunner->Start(Exe, FSteamCmdCommandLine::BuildLoginCheck(User), Error))
		{
			Notify(FText::FromString(Error), SNotificationItem::CS_Fail);
			LoginTestRunner.Reset();
			return;
		}
		Notify(LOCTEXT("TestStarted", "Testing the SteamCMD login (a first run may update SteamCMD)..."), SNotificationItem::CS_Pending, 4.f);
	}
}

#undef LOCTEXT_NAMESPACE
