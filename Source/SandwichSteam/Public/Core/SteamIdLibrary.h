// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Core/SteamId.h"
#include "SteamIdLibrary.generated.h"

/** Blueprint helpers for FSteamId. */
UCLASS()
class SANDWICHSTEAM_API USteamIdLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True when the Steam ID is not 0. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (ToolTip = "True when the Steam ID is valid (not 0)."))
	static bool IsValidSteamId(const FSteamId& SteamId);

	/** Decimal SteamID64 as text. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (DisplayName = "To String (Steam Id)", CompactNodeTitle = "->", BlueprintAutocast, ToolTip = "Converts a Steam ID to its decimal SteamID64 string."))
	static FString Conv_SteamIdToString(const FSteamId& SteamId);

	/** SteamID3 form such as [U:1:22202]. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (DisplayName = "To SteamID3", ToolTip = "Converts a Steam ID to its SteamID3 string, for example [U:1:22202]."))
	static FString ToSteamID3(const FSteamId& SteamId);

	/** Parses a SteamID64 or SteamID3 string. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (DisplayName = "Make Steam Id From String", ToolTip = "Parses a SteamID64 or SteamID3 string. Returns false and an invalid Steam ID when the text is not a Steam ID."))
	static bool MakeSteamIdFromString(const FString& String, FSteamId& SteamId);

	/** Equality. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (DisplayName = "Equal (Steam Id)", CompactNodeTitle = "==", Keywords = "== equal", ToolTip = "True when both Steam IDs are the same."))
	static bool EqualEqual_SteamId(const FSteamId& A, const FSteamId& B);

	/** Steam ID of the user logged in to the Steam client. Invalid when Steam is not ready. */
	UFUNCTION(BlueprintPure, Category = "Steam|Core", meta = (WorldContext = "WorldContextObject", ToolTip = "Returns the Steam ID of the local Steam user, or an invalid ID when Steam is not ready."))
	static FSteamId GetLocalSteamId(const UObject* WorldContextObject);
};
