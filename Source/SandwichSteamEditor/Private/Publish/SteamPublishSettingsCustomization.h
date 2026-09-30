// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Sandwich Steam - Publish (shared): shows the App ID (read only, comes from Sandwich Steam), the depot problems that stop a build, and
 * the Generate VDF (dry run) button.
 */
class FSteamPublishSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization
};

/** Sandwich Steam - Publish (User): SteamCMD path check plus the Open login terminal and Test login buttons. */
class FSteamPublishUserSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization
};
