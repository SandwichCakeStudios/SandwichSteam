// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamId.h"
#include "Core/SteamResult.h"
#include "Features/Overlay/SteamOverlayTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamOverlayLibrary.generated.h"

/** Blueprint entry points for the Steam overlay. Each returns a result that fails with a Steam.Error.* tag when Steam or the overlay is unavailable. */
UCLASS()
class SANDWICHSTEAM_API USteamOverlayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens a Steam overlay page that needs no argument (Friends, Community, Players, Settings, Official Game Group, Stats, Achievements, Store)."))
	static FSteamResult OpenSteamOverlay(const UObject* WorldContextObject, ESteamOverlayDialog Dialog);

	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the profile, chat or add-friend page of a Steam user, or the invite dialog of a lobby (Target is then the lobby ID)."))
	static FSteamResult OpenSteamOverlayTargetDialog(const UObject* WorldContextObject, ESteamOverlayDialog Dialog, FSteamId Target);

	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens the Steam store page of an app in the overlay. AppId 0 opens this game's page. Use it for DLC too."))
	static FSteamResult OpenSteamOverlayStore(const UObject* WorldContextObject, int32 AppId = 0, ESteamOverlayStoreFlag Flag = ESteamOverlayStoreFlag::None);

	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (WorldContext = "WorldContextObject", ToolTip = "Opens a web page in the Steam overlay browser."))
	static FSteamResult OpenSteamOverlayWebPage(const UObject* WorldContextObject, const FString& Url, bool bModal = false);

	UFUNCTION(BlueprintPure, Category = "Steam|Overlay", meta = (WorldContext = "WorldContextObject", ToolTip = "True when the Steam overlay is available: the game was started through Steam and the overlay is enabled."))
	static bool IsSteamOverlayEnabled(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Steam|Overlay", meta = (WorldContext = "WorldContextObject", ToolTip = "True while the Steam overlay is open."))
	static bool IsSteamOverlayActive(const UObject* WorldContextObject);
};
