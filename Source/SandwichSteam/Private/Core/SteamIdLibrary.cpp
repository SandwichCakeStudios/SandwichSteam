// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamIdLibrary.h"
#include "Core/SteamBackend.h"
#include "Core/SteamSDK.h"

bool USteamIdLibrary::IsValidSteamId(const FSteamId& SteamId)
{
	return SteamId.IsValid();
}

FString USteamIdLibrary::Conv_SteamIdToString(const FSteamId& SteamId)
{
	return SteamId.ToString();
}

FString USteamIdLibrary::ToSteamID3(const FSteamId& SteamId)
{
	return SteamId.ToSteamID3();
}

bool USteamIdLibrary::MakeSteamIdFromString(const FString& String, FSteamId& SteamId)
{
	return FSteamId::FromString(String, SteamId);
}

bool USteamIdLibrary::EqualEqual_SteamId(const FSteamId& A, const FSteamId& B)
{
	return A == B;
}

FSteamId USteamIdLibrary::GetLocalSteamId(const UObject* WorldContextObject)
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (SandwichSteam::IsSteamClientReady(WorldContextObject))
	{
		return FSteamId(static_cast<int64>(SteamUser()->GetSteamID().ConvertToUint64()));
	}
#endif
	return FSteamId();
}
