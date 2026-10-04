// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSandwichSteamModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	static FSandwichSteamModule& Get();
	static bool IsAvailable();

private:
	/** Registers Config/Tags/ of this plugin with the Gameplay Tags Manager. */
	void RegisterTagIniSearchPath();

	void HandlePostEngineInit();

	/** Logs a Warning when Steam runs another App ID than USteamToolSettings::SteamAppId (stale SteamDevAppId / steam_appid.txt). */
	void WarnOnAppIdMismatch();

	FDelegateHandle PostEngineInitHandle;
};
