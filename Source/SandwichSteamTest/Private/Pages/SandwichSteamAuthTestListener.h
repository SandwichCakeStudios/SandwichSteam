// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "SteamAuthTypes.h"
#include "UObject/Object.h"
#include "SandwichSteamAuthTestListener.generated.h"

/**
 * Receives the Auth subsystem's dynamic events for the Auth test page (Slate widgets cannot bind dynamic delegates).
 * Owned by the page through a TStrongObjectPtr; the page clears the callbacks before it goes away.
 */
UCLASS(Transient)
class USandwichSteamAuthTestListener : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void(const FSteamAuthResult&)> OnResult;
	TFunction<void(FSteamId)> OnFailed;

	UFUNCTION()
	void HandleResult(const FSteamAuthResult& Result)
	{
		if (OnResult)
		{
			OnResult(Result);
		}
	}

	UFUNCTION()
	void HandleFailed(FSteamId Player)
	{
		if (OnFailed)
		{
			OnFailed(Player);
		}
	}
};
