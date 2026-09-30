// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/SteamDebugCommandSet.h"
#include "Modules/ModuleManager.h"

class FSandwichSteamCloudModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugCommandSet Commands;
#endif
};
