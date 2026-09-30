// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamPresenceBatch.h"

using SandwichSteam::Presence::EIssue;

TMap<FString, FString> FSteamPresenceBatch::BuildDesired() const
{
	TMap<FString, FString> Desired;
	if (!bClearStaged)
	{
		Desired = Applied;
	}

	for (const FString& Key : StagedRemoves)
	{
		Desired.Remove(Key);
	}

	for (const TPair<FString, FString>& Pair : Staged)
	{
		Desired.Add(Pair.Key, Pair.Value);
	}

	return Desired;
}

int32 FSteamPresenceBatch::GetDesiredCount() const
{
	return BuildDesired().Num();
}

EIssue FSteamPresenceBatch::Set(const FString& Key, const FString& Value)
{
	const EIssue KeyIssue = SandwichSteam::Presence::CheckKey(Key);
	if (KeyIssue != EIssue::None)
	{
		return KeyIssue;
	}

	if (Value.IsEmpty())
	{
		return Remove(Key); // Steam treats an empty value as "remove the key".
	}

	const EIssue ValueIssue = SandwichSteam::Presence::CheckValue(Value);
	if (ValueIssue != EIssue::None)
	{
		return ValueIssue;
	}

	if (!Staged.Contains(Key))
	{
		const TMap<FString, FString> Desired = BuildDesired();
		if (!Desired.Contains(Key) && Desired.Num() >= SandwichSteam::Presence::MaxKeys)
		{
			return EIssue::TooManyKeys;
		}
	}

	StagedRemoves.Remove(Key);
	Staged.Add(Key, Value);
	return EIssue::None;
}

EIssue FSteamPresenceBatch::Remove(const FString& Key)
{
	if (Key.IsEmpty())
	{
		return EIssue::EmptyKey;
	}

	Staged.Remove(Key);
	StagedRemoves.Add(Key);
	return EIssue::None;
}

void FSteamPresenceBatch::Clear()
{
	Staged.Reset();
	StagedRemoves.Reset();
	bClearStaged = true;
}

FSteamPresenceBatch::FFlush FSteamPresenceBatch::Flush()
{
	FFlush Out;
	if (!HasPending())
	{
		return Out;
	}

	const TMap<FString, FString> Desired = BuildDesired();

	// A staged clear needs Steam's ClearRichPresence only when Steam has something to clear.
	if (bClearStaged && !Applied.IsEmpty())
	{
		Out.bClearAll = true;
		Applied.Reset();
	}

	for (const TPair<FString, FString>& Pair : Applied)
	{
		if (!Desired.Contains(Pair.Key))
		{
			Out.Removes.Add(Pair.Key);
		}
	}

	for (const TPair<FString, FString>& Pair : Desired)
	{
		const FString* Current = Applied.Find(Pair.Key);
		if (!Current || *Current != Pair.Value)
		{
			Out.Sets.Emplace(Pair.Key, Pair.Value);
		}
	}

	Applied = Desired;
	Staged.Reset();
	StagedRemoves.Reset();
	bClearStaged = false;
	return Out;
}

void FSteamPresenceBatch::RequeueApplied()
{
	for (const TPair<FString, FString>& Pair : Applied)
	{
		if (!Staged.Contains(Pair.Key) && !StagedRemoves.Contains(Pair.Key))
		{
			Staged.Add(Pair.Key, Pair.Value);
		}
	}
	Applied.Reset();
}
