// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SteamResult.generated.h"

/**
 * Common result of a Steam operation.
 * ErrorTag (a Steam.Error.* tag) is meant for game logic; NativeCode (Steam EResult or OSS code) is for diagnostics.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamResult
{
	GENERATED_BODY()

	/** True when the operation succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "True when the operation succeeded."))
	bool bSuccess = false;

	/** Steam.Error.* tag describing the failure. Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Steam.Error.* tag describing why the operation failed. Empty on success."))
	FGameplayTag ErrorTag;

	/** Human readable description, suitable for logs or UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Human readable description of the result."))
	FText Message;

	/** Native error code from Steam (EResult) or the Online Subsystem. 0 when not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Native Steam EResult or Online Subsystem error code, for diagnostics. 0 when not applicable."))
	int32 NativeCode = 0;

	bool IsSuccess() const { return bSuccess; }

	static FSteamResult Success()
	{
		FSteamResult Result;
		Result.bSuccess = true;
		return Result;
	}

	static FSteamResult Failure(const FGameplayTag& InErrorTag, const FText& InMessage = FText::GetEmpty(), int32 InNativeCode = 0)
	{
		FSteamResult Result;
		Result.bSuccess = false;
		Result.ErrorTag = InErrorTag;
		Result.Message = InMessage;
		Result.NativeCode = InNativeCode;
		return Result;
	}
};
