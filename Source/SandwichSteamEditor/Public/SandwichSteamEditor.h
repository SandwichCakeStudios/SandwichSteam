// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSandwichSteamEditor, Log, All);

class FSteamCookHelper;
struct FPropertyChangedEvent;

class FSandwichSteamEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	FSandwichSteamEditorModule();
	virtual ~FSandwichSteamEditorModule() override;

private:
	/** Adds Tools > Sandwich Steam > Open Steam Dashboard. */
	void RegisterMenus();

	/** Details customizations for the settings and the App Definition asset. */
	void RegisterDetailCustomizations();
	void UnregisterDetailCustomizations();

	/** Keeps the Message Log page up to date when the Sandwich Steam settings are edited. */
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);

	TUniquePtr<FSteamCookHelper> CookHelper;
	FDelegateHandle ObjectChangedHandle;
};
