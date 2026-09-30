// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamTestHUD.h"
#include "SandwichSteamTestSubsystem.h"

void ASandwichSteamTestHUD::BeginPlay()
{
	Super::BeginPlay();

	if (bShowOnBeginPlay)
	{
		if (USandwichSteamTestSubsystem* Subsystem = USandwichSteamTestSubsystem::Get(this))
		{
			Subsystem->ShowTestPanel();
		}
	}
}
