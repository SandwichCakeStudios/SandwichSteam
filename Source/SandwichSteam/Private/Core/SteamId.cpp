// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamId.h"
#include "Core/SteamBackend.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemTypes.h"

namespace
{
	// SteamID64 bit layout.
	constexpr uint64 AccountMask = 0xFFFFFFFFull;
	constexpr uint64 InstanceMask = 0xFFFFFull;
	constexpr int32 InstanceShift = 32;
	constexpr int32 TypeShift = 52;
	constexpr uint64 TypeMask = 0xFull;
	constexpr int32 UniverseShift = 56;
	constexpr uint64 UniverseMask = 0xFFull;

	// Chat instance flags (instance field of chat/lobby IDs).
	constexpr uint64 ChatInstanceClan = 0x80000ull;
	constexpr uint64 ChatInstanceLobby = 0x40000ull;

	// EAccountType values used here.
	constexpr uint64 TypeIndividual = 1;
	constexpr uint64 TypeMultiseat = 2;
	constexpr uint64 TypeGameServer = 3;
	constexpr uint64 TypeAnonGameServer = 4;
	constexpr uint64 TypePending = 5;
	constexpr uint64 TypeContentServer = 6;
	constexpr uint64 TypeClan = 7;
	constexpr uint64 TypeChat = 8;
	constexpr uint64 TypeConsoleUser = 9;
	constexpr uint64 TypeAnonUser = 10;

	bool IsAllDigits(const FString& String)
	{
		if (String.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Char : String)
		{
			if (!FChar::IsDigit(Char))
			{
				return false;
			}
		}
		return true;
	}

	bool TypeFromLetter(const TCHAR Letter, uint64& OutType, uint64& OutInstanceFlags, bool& bOutDefaultInstance)
	{
		OutInstanceFlags = 0;
		bOutDefaultInstance = false;
		switch (Letter)
		{
		case TEXT('U'): OutType = TypeIndividual; bOutDefaultInstance = true; return true;
		case TEXT('M'): OutType = TypeMultiseat; return true;
		case TEXT('G'): OutType = TypeGameServer; return true;
		case TEXT('A'): OutType = TypeAnonGameServer; return true;
		case TEXT('P'): OutType = TypePending; return true;
		case TEXT('C'): OutType = TypeContentServer; return true;
		case TEXT('g'): OutType = TypeClan; return true;
		case TEXT('T'): OutType = TypeChat; return true;
		case TEXT('L'): OutType = TypeChat; OutInstanceFlags = ChatInstanceLobby; return true;
		case TEXT('c'): OutType = TypeChat; OutInstanceFlags = ChatInstanceClan; return true;
		case TEXT('I'): OutType = TypeConsoleUser; bOutDefaultInstance = true; return true;
		case TEXT('a'): OutType = TypeAnonUser; return true;
		default: return false;
		}
	}

	TCHAR LetterFromType(uint64 Type, uint64 Instance)
	{
		switch (Type)
		{
		case TypeIndividual: return TEXT('U');
		case TypeMultiseat: return TEXT('M');
		case TypeGameServer: return TEXT('G');
		case TypeAnonGameServer: return TEXT('A');
		case TypePending: return TEXT('P');
		case TypeContentServer: return TEXT('C');
		case TypeClan: return TEXT('g');
		case TypeChat:
			if (Instance & ChatInstanceClan) { return TEXT('c'); }
			if (Instance & ChatInstanceLobby) { return TEXT('L'); }
			return TEXT('T');
		case TypeConsoleUser: return TEXT('I');
		case TypeAnonUser: return TEXT('a');
		default: return TEXT('i');
		}
	}
}

FString FSteamId::ToString() const
{
	return IsValid() ? FString::Printf(TEXT("%llu"), static_cast<uint64>(Value)) : FString();
}

FString FSteamId::ToSteamID3() const
{
	if (!IsValid())
	{
		return FString();
	}

	const uint64 Raw = static_cast<uint64>(Value);
	const uint64 Account = Raw & AccountMask;
	const uint64 Instance = (Raw >> InstanceShift) & InstanceMask;
	const uint64 Type = (Raw >> TypeShift) & TypeMask;
	const uint64 Universe = (Raw >> UniverseShift) & UniverseMask;

	return FString::Printf(TEXT("[%c:%llu:%llu]"), LetterFromType(Type, Instance), Universe, Account);
}

bool FSteamId::FromString(const FString& InString, FSteamId& OutId)
{
	OutId = FSteamId();

	const FString Trimmed = InString.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}

	// Decimal SteamID64.
	if (IsAllDigits(Trimmed))
	{
		if (Trimmed.Len() > 20)
		{
			return false;
		}
		OutId.Value = static_cast<int64>(FCString::Strtoui64(*Trimmed, nullptr, 10));
		return OutId.IsValid();
	}

	// SteamID3: [<letter>:<universe>:<account>] or [<letter>:<universe>:<account>:<instance>]
	if (Trimmed.Len() < 7 || !Trimmed.StartsWith(TEXT("[")) || !Trimmed.EndsWith(TEXT("]")))
	{
		return false;
	}

	TArray<FString> Parts;
	Trimmed.Mid(1, Trimmed.Len() - 2).ParseIntoArray(Parts, TEXT(":"), false);
	if ((Parts.Num() != 3 && Parts.Num() != 4) || Parts[0].Len() != 1 || !IsAllDigits(Parts[1]) || !IsAllDigits(Parts[2]))
	{
		return false;
	}

	uint64 Type = 0;
	uint64 InstanceFlags = 0;
	bool bDefaultInstance = false;
	if (!TypeFromLetter(Parts[0][0], Type, InstanceFlags, bDefaultInstance))
	{
		return false;
	}

	const uint64 Universe = FCString::Strtoui64(*Parts[1], nullptr, 10);
	const uint64 Account = FCString::Strtoui64(*Parts[2], nullptr, 10);
	if (Universe == 0 || Universe > UniverseMask || Account > AccountMask)
	{
		return false;
	}

	uint64 Instance = bDefaultInstance ? 1 : 0;
	Instance |= InstanceFlags;
	if (Parts.Num() == 4)
	{
		if (!IsAllDigits(Parts[3]))
		{
			return false;
		}
		Instance = FCString::Strtoui64(*Parts[3], nullptr, 10) | InstanceFlags;
		if (Instance > InstanceMask)
		{
			return false;
		}
	}

	OutId.Value = static_cast<int64>((Universe << UniverseShift) | (Type << TypeShift) | (Instance << InstanceShift) | Account);
	return OutId.IsValid();
}

bool FSteamId::FromUniqueNetId(const FUniqueNetId& NetId, FSteamId& OutId)
{
	OutId = FSteamId();

	if (NetId.GetType() != STEAM_SUBSYSTEM)
	{
		return false;
	}

	// [verify] FUniqueNetIdSteam::ToString() returns the decimal SteamID64 (FromString also accepts SteamID3).
	return FromString(NetId.ToString(), OutId);
}

TSharedPtr<const FUniqueNetId> FSteamId::ToUniqueNetId(const UObject* WorldContext) const
{
	if (!IsValid())
	{
		return nullptr;
	}

	IOnlineSubsystem* SteamOSS = SandwichSteam::GetSteamOSS(WorldContext);
	const IOnlineIdentityPtr Identity = SteamOSS ? SteamOSS->GetIdentityInterface() : nullptr;
	return Identity.IsValid() ? Identity->CreateUniquePlayerId(ToString()) : nullptr;
}
