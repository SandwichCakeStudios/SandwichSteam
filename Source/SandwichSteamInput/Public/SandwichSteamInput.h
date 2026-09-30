// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/SteamDebugCommandSet.h"
#include "Modules/ModuleManager.h"

class FSandwichSteamInputModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	/**
	 * Registers the SteamInput_ keys of the assigned App Definition, so Enhanced Input mapping contexts (and the editor's key picker) know them
	 * before the first game instance exists. The feature registers them again when it starts, so this only matters for editing.
	 */
	void RegisterKeysFromSettings();

	FDelegateHandle PostEngineInitHandle;
#if WITH_EDITOR
	FDelegateHandle ObjectChangedHandle;
	FDelegateHandle DefinitionChangedHandle;
#endif

#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugCommandSet Commands;
#endif
};
