// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamNameWarnings.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamLog.h"

namespace SandwichSteam
{
	namespace
	{
		constexpr int32 MaxRememberedNames = 256;
	}

	FSteamResult WarnOnceIfInvalid(const TCHAR* Feature, const FString& Name, const FSteamResult& Result)
	{
		// Steam reports an unknown name as InvalidArgument (plugin check) or Failed (Steam refused the call); the rest is state, not a typo.
		const bool bPointsAtName = Result.ErrorTag.MatchesTag(SteamGameplayTags::Error_InvalidArgument) || Result.ErrorTag.MatchesTag(SteamGameplayTags::Error_Failed);
		if (Result.IsSuccess() || !bPointsAtName)
		{
			return Result;
		}

		check(IsInGameThread());
		static TSet<FString> Warned;
		if (Warned.Num() >= MaxRememberedNames)
		{
			return Result;
		}

		bool bAlreadyWarned = false;
		Warned.Add(FString::Printf(TEXT("%s/%s"), Feature, *Name), &bAlreadyWarned);
		if (!bAlreadyWarned)
		{
			UE_LOG(LogSandwichSteam, Warning, TEXT("Steam %s '%s': %s (logged once per name)"), Feature, *Name, *Result.Message.ToString());
		}
		return Result;
	}
}
