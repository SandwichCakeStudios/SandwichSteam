// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamTestSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SandwichSteamTestWidget.h"

USandwichSteamTestSubsystem* USandwichSteamTestSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USandwichSteamTestSubsystem>() : nullptr;
}

void USandwichSteamTestSubsystem::ShowTestPanel()
{
	if (Widget)
	{
		return;
	}

	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (!PlayerController)
	{
		return;
	}

	Widget = CreateWidget<USandwichSteamTestWidget>(PlayerController, USandwichSteamTestWidget::StaticClass());
	if (!Widget)
	{
		return;
	}

	Widget->OnCloseRequested.BindUObject(this, &USandwichSteamTestSubsystem::HideTestPanel);
	Widget->AddToViewport(1000);

	PlayerController->SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	PlayerController->bShowMouseCursor = true;
}

void USandwichSteamTestSubsystem::HideTestPanel()
{
	if (!Widget)
	{
		return;
	}

	Widget->OnCloseRequested.Unbind();
	Widget->RemoveFromParent();
	Widget = nullptr;

	if (APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr)
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->bShowMouseCursor = false;
	}
}

void USandwichSteamTestSubsystem::ToggleTestPanel()
{
	if (Widget)
	{
		HideTestPanel();
	}
	else
	{
		ShowTestPanel();
	}
}
