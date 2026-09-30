// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Features/User/SteamUserTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamUserLibrary.generated.h"

class UTexture2D;

/** Synchronous Blueprint getters for the local Steam user. They return defaults (empty, 0, false) while Steam is not active. */
UCLASS()
class SANDWICHSTEAM_API USteamUserLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "Display (persona) name of the local Steam user. Empty when Steam is not active."))
	static FString GetSteamPersonaName(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam level of the local user. 0 when unknown or Steam is not active."))
	static int32 GetSteamLevel(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the Steam client is logged on."))
	static bool IsSteamLoggedOn(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the local user owns this game, including through Family Sharing."))
	static bool IsSteamSubscribed(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the local user owns the given app or DLC."))
	static bool IsSteamSubscribedApp(const UObject* WorldContextObject, int32 AppId);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the local user plays this game through Family Sharing."))
	static bool IsSteamFamilySharedLicense(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "Steam ID of the account that owns this game's license. Differs from the local user for Family Sharing."))
	static FSteamId GetSteamAppOwner(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "When the local user first purchased the given app (UTC). 1970-01-01 when never purchased."))
	static FDateTime GetSteamEarliestPurchaseTime(const UObject* WorldContextObject, int32 AppId);

	/** Avatar if it is already cached, otherwise null. Use the Get Steam Avatar node to load one. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User", meta = (WorldContext = "WorldContextObject", ToolTip = "Returns the avatar if it is already cached, otherwise null. Use the Get Steam Avatar node to load one."))
	static UTexture2D* GetCachedSteamAvatar(const UObject* WorldContextObject, FSteamId UserId, ESteamAvatarSize Size = ESteamAvatarSize::Medium);
};
