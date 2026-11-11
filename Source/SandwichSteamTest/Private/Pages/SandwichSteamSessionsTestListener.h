// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "SteamSessionTypes.h"
#include "UObject/Object.h"
#include "SandwichSteamSessionsTestListener.generated.h"

/**
 * Receives the Sessions subsystem's dynamic join events for the Sessions test page (Slate widgets cannot bind dynamic delegates).
 * Owned by the page through a TStrongObjectPtr; the page clears the callbacks before it goes away.
 */
UCLASS(Transient)
class USandwichSteamSessionsTestListener : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void(const FSteamJoinRequest&)> OnJoinRequested;
	TFunction<void(const FSteamResult&, const FString&)> OnJoinFinished;

	UFUNCTION()
	void HandleJoinRequested(const FSteamJoinRequest& Request)
	{
		if (OnJoinRequested)
		{
			OnJoinRequested(Request);
		}
	}

	UFUNCTION()
	void HandleJoinFinished(const FSteamResult& Result, const FString& ConnectString)
	{
		if (OnJoinFinished)
		{
			OnJoinFinished(Result, ConnectString);
		}
	}
};
