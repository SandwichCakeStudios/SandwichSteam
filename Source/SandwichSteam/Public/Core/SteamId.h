// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SteamId.generated.h"

class FUniqueNetId;

/**
 * 64-bit Steam ID (user, lobby, clan, game server, ...) that Blueprint can carry.
 * Blueprint has no uint64 and FUniqueNetIdRepl is opaque to designers, so this is the common identity type of the plugin.
 * Bit layout: 0-31 account, 32-51 instance, 52-55 type, 56-63 universe.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamId
{
	GENERATED_BODY()

	/** The raw 64-bit Steam ID. 0 is invalid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steam", meta = (ToolTip = "The raw 64-bit Steam ID. 0 is invalid."))
	int64 Value = 0;

	FSteamId() = default;

	explicit FSteamId(int64 InValue)
		: Value(InValue)
	{
	}

	bool IsValid() const { return Value != 0; }

	/** Decimal SteamID64, e.g. "76561197960287930". Empty when invalid. */
	FString ToString() const;

	/**
	 * SteamID3 form, e.g. "[U:1:22202]". Exact for users and lobbies; other account types are best effort.
	 * Empty when invalid.
	 */
	FString ToSteamID3() const;

	/** Parses a decimal SteamID64 or a SteamID3 string ("[U:1:22202]"). Returns false and an invalid Out on failure. */
	static bool FromString(const FString& InString, FSteamId& OutId);

	/** SteamID64 for a Steam unique net id created by the Steam Online Subsystem. Returns false for other net id types. */
	static bool FromUniqueNetId(const FUniqueNetId& NetId, FSteamId& OutId);

	/** Steam unique net id for this ID via the Steam Online Subsystem of the world. Null when Steam OSS is unavailable or the ID is invalid. */
	TSharedPtr<const FUniqueNetId> ToUniqueNetId(const UObject* WorldContext) const;

	friend bool operator==(const FSteamId& A, const FSteamId& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSteamId& A, const FSteamId& B) { return A.Value != B.Value; }
	friend uint32 GetTypeHash(const FSteamId& Id) { return ::GetTypeHash(Id.Value); }
};
