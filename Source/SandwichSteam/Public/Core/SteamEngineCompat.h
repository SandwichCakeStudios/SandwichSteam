// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersionComparison.h"

/**
 * Automation test application context mask (5.5 moved it out of EAutomationTestFlags).
 * A macro so it can be passed straight into IMPLEMENT_SIMPLE_AUTOMATION_TEST.
 */
#if UE_VERSION_OLDER_THAN(5, 5, 0)
	#define SANDWICHSTEAM_TEST_CONTEXT_MASK EAutomationTestFlags::ApplicationContextMask
#else
	#define SANDWICHSTEAM_TEST_CONTEXT_MASK EAutomationTestFlags_ApplicationContextMask
#endif

/**
 * Engine version shims. The plugin supports UE 5.4 - 5.8; every engine API difference is wrapped here
 * so modules never carry their own version checks. Use UE_VERSION_OLDER_THAN (present in every supported version).
 */
namespace SandwichSteam::Compat
{
	/** FCoreDelegates post engine init delegate (5.8 replaced the static member with an accessor). */
	FORCEINLINE auto& OnPostEngineInit()
	{
#if UE_VERSION_OLDER_THAN(5, 8, 0)
		return FCoreDelegates::OnPostEngineInit;
#else
		return FCoreDelegates::GetOnPostEngineInit();
#endif
	}
}
