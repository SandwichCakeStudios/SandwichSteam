// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SandwichSteamTestSubsystem.generated.h"

class USandwichSteamTestWidget;

/** Shows and hides the test panel for one GameInstance and switches the input mode so the mouse works. */
UCLASS()
class SANDWICHSTEAMTEST_API USandwichSteamTestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static USandwichSteamTestSubsystem* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Steam|Test", meta = (ToolTip = "Shows the Sandwich Steam test panel and enables the mouse cursor."))
	void ShowTestPanel();

	UFUNCTION(BlueprintCallable, Category = "Steam|Test", meta = (ToolTip = "Hides the Sandwich Steam test panel and gives input back to the game."))
	void HideTestPanel();

	UFUNCTION(BlueprintCallable, Category = "Steam|Test", meta = (ToolTip = "Shows the test panel when hidden, hides it when shown."))
	void ToggleTestPanel();

	UFUNCTION(BlueprintPure, Category = "Steam|Test", meta = (ToolTip = "True while the test panel is on screen."))
	bool IsTestPanelVisible() const { return Widget != nullptr; }

private:
	UPROPERTY(Transient)
	TObjectPtr<USandwichSteamTestWidget> Widget;
};
