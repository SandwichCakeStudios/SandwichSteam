// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/Utility/SteamUtilityAsyncActions.h"
#include "Core/SteamGameplayTags.h"
#include "Features/Utility/SteamUtilitySubsystem.h"

USteamShowGamepadTextInputAsyncAction* USteamShowGamepadTextInputAsyncAction::ShowSteamGamepadTextInput(const UObject* WorldContextObject, FText Description,
	int32 MaxCharacters, FString ExistingText, ESteamGamepadTextInputMode InputMode, ESteamGamepadTextLineMode LineMode)
{
	USteamShowGamepadTextInputAsyncAction* Action = NewObject<USteamShowGamepadTextInputAsyncAction>();
	Action->SetWorldContext(WorldContextObject);
	Action->TimeoutSeconds = 0.0f; // The user decides when the input ends.
	Action->Description = MoveTemp(Description);
	Action->MaxCharacters = MaxCharacters;
	Action->ExistingText = MoveTemp(ExistingText);
	Action->InputMode = InputMode;
	Action->LineMode = LineMode;
	return Action;
}

TSubclassOf<USteamFeatureSubsystem> USteamShowGamepadTextInputAsyncAction::GetFeatureClass() const
{
	return USteamUtilitySubsystem::StaticClass();
}

void USteamShowGamepadTextInputAsyncAction::StartRequest()
{
	USteamUtilitySubsystem* Utility = Cast<USteamUtilitySubsystem>(GetFeature());
	if (!Utility)
	{
		return;
	}

	const FSteamResult Started = Utility->ShowGamepadTextInput(InputMode, LineMode, Description, MaxCharacters, ExistingText,
		FSteamTextInputDelegate::CreateWeakLambda(this, [this](bool bSubmitted, const FString& Text)
		{
			if (bSubmitted)
			{
				SubmittedText = Text;
				FinishSuccess();
			}
			else
			{
				FinishFailure(FSteamResult::Failure(SteamGameplayTags::Error_Cancelled, NSLOCTEXT("SandwichSteam", "TextInputCancelled", "The user dismissed the text input.")));
			}
		}));

	if (!Started.IsSuccess())
	{
		FinishFailure(Started);
	}
}

void USteamShowGamepadTextInputAsyncAction::BroadcastSuccess()
{
	OnTextSubmitted.Broadcast(SubmittedText);
	Super::BroadcastSuccess();
}
