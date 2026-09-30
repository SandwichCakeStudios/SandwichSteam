// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Project Settings > Plugins > Sandwich Steam.
 * Adds the status strip (validator results with one-click fixes, Configure Steam, steam_appid.txt) at the top and replaces the
 * DisabledFeatures tag container with one checkbox per installed feature, each with a scope badge.
 */
class FSteamToolSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization
};
