// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Naming rules of Steam Input actions and the keys the Input module registers for them. Pure (no SDK, no engine state), shared by the
 * App Definition validator, the Input module and the editor's action file generator.
 */
namespace SandwichSteam::Input
{
	/** Every Steam Input key is named SteamInput_<Action>. Analog sticks add _X and _Y for the axes (SteamInput_<Action> is the 2D pair). */
	inline const TCHAR* const KeyPrefix = TEXT("SteamInput_");

	/** Longest action or action set name. */
	constexpr int32 MaxNameLength = 64;

	enum class ENameIssue : uint8
	{
		None,
		Empty,
		TooLong,
		BadCharacter,
		BadStart,
		ReservedSuffix
	};

	/**
	 * Action and action set names use letters, digits and underscores, start with a letter, and are at most 64 characters.
	 * Action names must not end in _X or _Y (those suffixes name the axes of an analog stick key); bIsAction turns that check on.
	 */
	inline ENameIssue CheckName(const FString& Name, bool bIsAction)
	{
		if (Name.IsEmpty())
		{
			return ENameIssue::Empty;
		}
		if (Name.Len() > MaxNameLength)
		{
			return ENameIssue::TooLong;
		}
		if (!(FChar::IsAlpha(Name[0]) && Name[0] < 128))
		{
			return ENameIssue::BadStart;
		}
		for (const TCHAR Char : Name)
		{
			if (!(FChar::IsAlnum(Char) && Char < 128) && Char != TEXT('_'))
			{
				return ENameIssue::BadCharacter;
			}
		}
		if (bIsAction && (Name.EndsWith(TEXT("_X"), ESearchCase::CaseSensitive) || Name.EndsWith(TEXT("_Y"), ESearchCase::CaseSensitive)))
		{
			return ENameIssue::ReservedSuffix;
		}
		return ENameIssue::None;
	}

	/** English description for logs and validation messages. */
	inline const TCHAR* DescribeIssue(ENameIssue Issue)
	{
		switch (Issue)
		{
		case ENameIssue::Empty:
			return TEXT("the name is empty");
		case ENameIssue::TooLong:
			return TEXT("the name is longer than 64 characters");
		case ENameIssue::BadCharacter:
			return TEXT("use only letters, digits and underscores");
		case ENameIssue::BadStart:
			return TEXT("the name must start with a letter");
		case ENameIssue::ReservedSuffix:
			return TEXT("action names must not end in _X or _Y (reserved for the axes of a stick)");
		default:
			return TEXT("ok");
		}
	}

	/** SteamInput_<Action>: the key of a button or trigger, and the 2D pair key of a stick. */
	inline FName MakeKeyName(FName Action)
	{
		return FName(*(FString(KeyPrefix) + Action.ToString()));
	}

	/** SteamInput_<Action>_X or _Y: the axes of a stick. */
	inline FName MakeAxisKeyName(FName Action, bool bYAxis)
	{
		return FName(*(FString(KeyPrefix) + Action.ToString() + (bYAxis ? TEXT("_Y") : TEXT("_X"))));
	}

	/** Cuts the prefix off a key name. Returns false when the key is not a Steam Input key. The result may still carry an _X / _Y axis suffix. */
	inline bool StripKeyPrefix(FName Key, FString& OutName)
	{
		const FString KeyString = Key.ToString();
		if (!KeyString.StartsWith(KeyPrefix, ESearchCase::CaseSensitive) || KeyString.Len() <= FCString::Strlen(KeyPrefix))
		{
			return false;
		}
		OutName = KeyString.RightChop(FCString::Strlen(KeyPrefix));
		return true;
	}
}
