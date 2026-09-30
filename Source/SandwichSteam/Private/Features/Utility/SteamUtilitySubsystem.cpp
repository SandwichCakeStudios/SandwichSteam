// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Utility/SteamUtilitySubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/Utility/Backend/SteamUtilityBackend.h"
#include "Features/Utility/SteamLanguage.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

USteamUtilitySubsystem* USteamUtilitySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamUtilitySubsystem>() : nullptr;
}

FGameplayTag USteamUtilitySubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Utility;
}

bool USteamUtilitySubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	const USteamCoreSubsystem* SteamCoreSubsystem = GetCore();
	const TSharedPtr<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher = SteamCoreSubsystem ? SteamCoreSubsystem->GetDispatcher() : nullptr;
	if (!Dispatcher.IsValid())
	{
		return false;
	}

	Backend = MakeShared<FSteamUtilityBackend>(this, Dispatcher.ToSharedRef());

	const USteamToolSettings* Settings = USteamToolSettings::Get();
	if (Settings && Settings->bApplySteamLanguageOnStart)
	{
		ApplySteamLanguage();
	}
	return true;
#else
	return false;
#endif
}

void USteamUtilitySubsystem::ShutdownFeature()
{
	if (Backend.IsValid())
	{
		Backend->DismissFloatingGamepadTextInput();
	}

	// The async action observes OnFeatureActiveChanged and reports Steam.Error.Cancelled.
	PendingTextInput.Unbind();
	Backend.Reset();
}

void USteamUtilitySubsystem::ApplySteamLanguage() const
{
	FString SteamLanguage = Backend->GetGameLanguage();
	if (SteamLanguage.IsEmpty())
	{
		SteamLanguage = Backend->GetSteamUiLanguage();
	}

	FString Culture;
	if (!SandwichSteam::SteamLanguageToCulture(SteamLanguage, Culture))
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("bApplySteamLanguageOnStart: unknown Steam language '%s', culture unchanged."), *SteamLanguage);
		return;
	}

	FInternationalization& Internationalization = FInternationalization::Get();
	if (!Internationalization.GetCulture(Culture).IsValid())
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("bApplySteamLanguageOnStart: culture '%s' (Steam language '%s') is not available, culture unchanged."), *Culture, *SteamLanguage);
		return;
	}

	if (Internationalization.SetCurrentCulture(Culture))
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Culture set to '%s' from the Steam language '%s'."), *Culture, *SteamLanguage);
	}
	else
	{
		UE_LOG(LogSandwichSteam, Warning, TEXT("Could not set culture '%s' (Steam language '%s')."), *Culture, *SteamLanguage);
	}
}

int32 USteamUtilitySubsystem::GetAppId() const
{
	FSteamResult Result;
	return RequireActive(Result) ? static_cast<int32>(Backend->GetAppId()) : 0;
}

FString USteamUtilitySubsystem::GetIpCountry() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetIpCountry() : FString();
}

FString USteamUtilitySubsystem::GetSteamUiLanguage() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetSteamUiLanguage() : FString();
}

FString USteamUtilitySubsystem::GetCurrentGameLanguage() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetGameLanguage() : FString();
}

FDateTime USteamUtilitySubsystem::GetServerRealTime() const
{
	FSteamResult Result;
	return FDateTime::FromUnixTimestamp(RequireActive(Result) ? Backend->GetServerRealTime() : 0);
}

int32 USteamUtilitySubsystem::GetSecondsSinceAppActive() const
{
	FSteamResult Result;
	return RequireActive(Result) ? Backend->GetSecondsSinceAppActive() : 0;
}

bool USteamUtilitySubsystem::IsRunningOnSteamDeck() const
{
	FSteamResult Result;
	return RequireActive(Result) && Backend->IsRunningOnSteamDeck();
}

bool USteamUtilitySubsystem::IsBigPictureMode() const
{
	FSteamResult Result;
	return RequireActive(Result) && Backend->IsBigPictureMode();
}

FSteamResult USteamUtilitySubsystem::ShowGamepadTextInput(ESteamGamepadTextInputMode InputMode, ESteamGamepadTextLineMode LineMode, const FText& Description,
	int32 MaxCharacters, const FString& ExistingText, FSteamTextInputDelegate OnDismissed)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (PendingTextInput.IsBound())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "TextInputBusy", "A Steam text input is already open."));
	}

	const uint32 MaxChars = static_cast<uint32>(FMath::Max(1, MaxCharacters));
	if (!Backend->ShowGamepadTextInput(static_cast<int32>(InputMode), static_cast<int32>(LineMode), Description.ToString(), MaxChars, ExistingText))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
			NSLOCTEXT("SandwichSteam", "TextInputUnsupported", "Steam can only show the on-screen text input in Big Picture mode or on the Steam Deck."));
	}

	PendingTextInput = MoveTemp(OnDismissed);
	return FSteamResult::Success();
}

FSteamResult USteamUtilitySubsystem::ShowFloatingGamepadTextInput(ESteamFloatingKeyboardMode Mode, FIntPoint FieldPosition, FIntPoint FieldSize)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (!Backend->ShowFloatingGamepadTextInput(static_cast<int32>(Mode), FieldPosition.X, FieldPosition.Y, FieldSize.X, FieldSize.Y))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported,
			NSLOCTEXT("SandwichSteam", "FloatingKeyboardUnsupported", "Steam could not show the floating on-screen keyboard (Steam Deck / Big Picture only)."));
	}
	return FSteamResult::Success();
}

void USteamUtilitySubsystem::DismissFloatingGamepadTextInput()
{
	FSteamResult Result;
	if (RequireActive(Result))
	{
		Backend->DismissFloatingGamepadTextInput();
	}
}

void USteamUtilitySubsystem::HandleGamepadTextInputDismissed(bool bSubmitted, uint32 SubmittedLength)
{
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return;
	}

	// Move first: the callback may open another input.
	FSteamTextInputDelegate Callback = MoveTemp(PendingTextInput);
	PendingTextInput.Unbind();

	const FString Text = bSubmitted ? Backend->GetEnteredGamepadText(SubmittedLength) : FString();
	Callback.ExecuteIfBound(bSubmitted, Text);
}

void USteamUtilitySubsystem::HandleFloatingKeyboardDismissed()
{
	if (IsFeatureActive())
	{
		OnFloatingGamepadTextInputDismissed.Broadcast();
	}
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamUtilitySubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Utility: %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive() || !Backend.IsValid())
	{
		return Report;
	}

	Report += FString::Printf(TEXT("  AppId: %u, IP country: %s\n"), Backend->GetAppId(), *Backend->GetIpCountry());
	Report += FString::Printf(TEXT("  Steam UI language: %s, game language: %s\n"), *Backend->GetSteamUiLanguage(), *Backend->GetGameLanguage());
	Report += FString::Printf(TEXT("  Server time (UTC): %s, seconds since app active: %d\n"),
		*FDateTime::FromUnixTimestamp(Backend->GetServerRealTime()).ToString(), Backend->GetSecondsSinceAppActive());
	Report += FString::Printf(TEXT("  Steam Deck: %s, Big Picture: %s, text input open: %s\n"),
		Backend->IsRunningOnSteamDeck() ? TEXT("yes") : TEXT("no"), Backend->IsBigPictureMode() ? TEXT("yes") : TEXT("no"),
		PendingTextInput.IsBound() ? TEXT("yes") : TEXT("no"));
	return Report;
}
#endif
