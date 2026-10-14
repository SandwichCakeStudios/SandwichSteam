// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Editor access to the optional Voice module's settings (Project Settings > Plugins > Sandwich Steam - Voice).
 * The editor does not depend on feature modules, so the settings object is found by class path and read through reflection.
 */
namespace SandwichSteam::Editor::Voice
{
	/** The Use Steam Voice Pipeline property, for property change filters. */
	extern const FName UseSteamPipelinePropertyName;

	/** True when the SandwichSteamVoice module is loaded. */
	bool IsModuleInstalled();

	/** True when Object is the Voice settings default object. */
	bool IsSettingsObject(const UObject* Object);

	/** Use Steam Voice Pipeline, as Project Settings shows it right now. True when the module or the property is missing (the default). */
	bool UsesSteamPipeline();
}
