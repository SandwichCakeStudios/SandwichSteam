// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "SteamUserTypes.generated.h"

class UTexture2D;

/** Size of a Steam avatar image. */
UENUM(BlueprintType)
enum class ESteamAvatarSize : uint8
{
	/** 32x32 */
	Small UMETA(DisplayName = "Small (32x32)"),
	/** 64x64 */
	Medium UMETA(DisplayName = "Medium (64x64)"),
	/** 184x184 */
	Large UMETA(DisplayName = "Large (184x184)")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamPersonaNameChanged, const FString&, NewName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamAvatarChanged, FSteamId, UserId);

/** C++ completion of an avatar request. The texture is null on failure. Runs on the game thread. */
DECLARE_DELEGATE_TwoParams(FSteamAvatarResultDelegate, const FSteamResult& /*Result*/, UTexture2D* /*Avatar*/);

/** C++ completion of a Web API ticket request. TicketHex is the ticket as a hex string; TicketHandle is needed to cancel it. */
DECLARE_DELEGATE_ThreeParams(FSteamWebApiTicketDelegate, const FSteamResult& /*Result*/, const FString& /*TicketHex*/, int32 /*TicketHandle*/);
