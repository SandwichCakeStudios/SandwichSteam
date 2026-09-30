// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SandwichSteamTestHUD.generated.h"

/** Set this as the HUD class of a Game Mode to open the Steam test panel on begin play. Without it use the console command Steam.Test.Toggle. */
UCLASS()
class SANDWICHSTEAMTEST_API ASandwichSteamTestHUD : public AHUD
{
	GENERATED_BODY()

public:
	/** Open the test panel automatically when the game starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam|Test", meta = (ToolTip = "Open the Steam test panel automatically when the game starts."))
	bool bShowOnBeginPlay = true;

protected:
	virtual void BeginPlay() override;
};
