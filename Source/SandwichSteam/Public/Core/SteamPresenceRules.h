// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Limits and reserved keys of Steam rich presence. Pure (no SDK), shared by the App Definition validator and the
 * Presence module. The numbers are Steam's (k_cchMaxRichPresenceKeys / KeyLength / ValueLength).
 */
namespace SandwichSteam::Presence
{
	/** Most keys one user can have set at the same time. */
	constexpr int32 MaxKeys = 30;

	/** Longest key, in UTF-8 bytes. */
	constexpr int32 MaxKeyBytes = 64;

	/** Longest value, in UTF-8 bytes. */
	constexpr int32 MaxValueBytes = 256;

	/** Keys Steam gives a meaning to. */
	inline const TCHAR* const KeyDisplay = TEXT("steam_display");
	inline const TCHAR* const KeyPlayerGroup = TEXT("steam_player_group");
	inline const TCHAR* const KeyPlayerGroupSize = TEXT("steam_player_group_size");
	inline const TCHAR* const KeyConnect = TEXT("connect");

	enum class EIssue : uint8
	{
		None,
		EmptyKey,
		KeyTooLong,
		ValueTooLong,
		TooManyKeys
	};

	inline int32 Utf8Length(const FString& Text)
	{
		return FTCHARToUTF8(*Text).Length();
	}

	inline EIssue CheckKey(const FString& Key)
	{
		if (Key.IsEmpty())
		{
			return EIssue::EmptyKey;
		}
		return Utf8Length(Key) > MaxKeyBytes ? EIssue::KeyTooLong : EIssue::None;
	}

	inline EIssue CheckValue(const FString& Value)
	{
		return Utf8Length(Value) > MaxValueBytes ? EIssue::ValueTooLong : EIssue::None;
	}

	/** True for the keys Steam reads itself (steam_display, steam_player_group, steam_player_group_size, connect). */
	inline bool IsReservedKey(const FString& Key)
	{
		return Key.Equals(KeyDisplay, ESearchCase::CaseSensitive)
			|| Key.Equals(KeyPlayerGroup, ESearchCase::CaseSensitive)
			|| Key.Equals(KeyPlayerGroupSize, ESearchCase::CaseSensitive)
			|| Key.Equals(KeyConnect, ESearchCase::CaseSensitive);
	}

	/** English description for logs. */
	inline const TCHAR* DescribeIssue(EIssue Issue)
	{
		switch (Issue)
		{
		case EIssue::EmptyKey:
			return TEXT("the key is empty");
		case EIssue::KeyTooLong:
			return TEXT("the key is longer than 64 bytes");
		case EIssue::ValueTooLong:
			return TEXT("the value is longer than 256 bytes");
		case EIssue::TooManyKeys:
			return TEXT("Steam allows at most 30 rich presence keys");
		default:
			return TEXT("ok");
		}
	}
}
