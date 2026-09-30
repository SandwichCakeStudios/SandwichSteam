// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamResult.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamDLCTypes.h"
#include "SteamDLCLibrary.generated.h"

/** DLC helpers for Blueprints. Checks return false / empty while the DLC feature is inactive. */
UCLASS()
class SANDWICHSTEAMDLC_API USteamDLCLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", Categories = "Steam.DLC", ToolTip = "True when the local user owns the DLC of the Steam App Definition with this tag."))
	static bool IsSteamDLCOwned(const UObject* WorldContextObject, FGameplayTag DLC);

	UFUNCTION(BlueprintPure, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", Categories = "Steam.DLC", ToolTip = "True when the files of the DLC are installed."))
	static bool IsSteamDLCInstalled(const UObject* WorldContextObject, FGameplayTag DLC);


	UFUNCTION(BlueprintCallable, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", ToolTip = "Every DLC Steam knows for this game with its owned and installed state and, when there is a row, its tag."))
	static TArray<FSteamDLCInfo> ListSteamDLC(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", Categories = "Steam.DLC", ToolTip = "Asks Steam to install an owned DLC. The DLC Installed event fires when it is done."))
	static FSteamResult InstallSteamDLC(const UObject* WorldContextObject, FGameplayTag DLC);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", Categories = "Steam.DLC", ToolTip = "Asks Steam to uninstall a DLC."))
	static FSteamResult UninstallSteamDLC(const UObject* WorldContextObject, FGameplayTag DLC);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", Categories = "Steam.DLC", ToolTip = "Download state of a DLC: whether it downloads and how many bytes are done."))
	static FSteamDLCProgress GetSteamDLCProgress(const UObject* WorldContextObject, FGameplayTag DLC);

	UFUNCTION(BlueprintPure, Category = "Steam|DLC", meta = (ToolTip = "0 to 1 progress of a DLC download. 0 when nothing downloads."))
	static float GetSteamDLCProgressFraction(const FSteamDLCProgress& Progress);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC", meta = (WorldContext = "WorldContextObject", Categories = "Steam.DLC", ToolTip = "Opens the store page of the DLC in the Steam overlay with the DLC in the cart."))
	static FSteamResult OpenSteamDLCStorePage(const UObject* WorldContextObject, FGameplayTag DLC);

	// By App ID: the same with the DLC's App ID from Steamworks instead of a tag. No App Definition row needed.
	// IsSteamDLCAppOwned keeps its function name so Blueprints saved before the By App ID nodes still load.

	UFUNCTION(BlueprintPure, Category = "Steam|DLC|By App ID", meta = (WorldContext = "WorldContextObject", DisplayName = "Is Steam DLC Owned By App ID", Keywords = "is steam dlc owned app id", ToolTip = "True when the local user owns the DLC with this App ID."))
	static bool IsSteamDLCAppOwned(const UObject* WorldContextObject, int32 AppId);

	UFUNCTION(BlueprintPure, Category = "Steam|DLC|By App ID", meta = (WorldContext = "WorldContextObject", DisplayName = "Is Steam DLC Installed By App ID", Keywords = "is steam dlc installed app id", ToolTip = "True when the files of the DLC with this App ID are installed."))
	static bool IsSteamDLCInstalledByAppId(const UObject* WorldContextObject, int32 AppId);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC|By App ID", meta = (WorldContext = "WorldContextObject", DisplayName = "Install Steam DLC By App ID", Keywords = "install download steam dlc app id", ToolTip = "Asks Steam to install the owned DLC with this App ID. The DLC Installed event fires when it is done."))
	static FSteamResult InstallSteamDLCByAppId(const UObject* WorldContextObject, int32 AppId);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC|By App ID", meta = (WorldContext = "WorldContextObject", DisplayName = "Uninstall Steam DLC By App ID", Keywords = "uninstall remove steam dlc app id", ToolTip = "Asks Steam to uninstall the DLC with this App ID."))
	static FSteamResult UninstallSteamDLCByAppId(const UObject* WorldContextObject, int32 AppId);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC|By App ID", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Steam DLC Progress By App ID", Keywords = "get steam dlc download progress app id", ToolTip = "Download state of the DLC with this App ID: whether it downloads and how many bytes are done."))
	static FSteamDLCProgress GetSteamDLCProgressByAppId(const UObject* WorldContextObject, int32 AppId);

	UFUNCTION(BlueprintCallable, Category = "Steam|DLC|By App ID", meta = (WorldContext = "WorldContextObject", DisplayName = "Open Steam DLC Store Page By App ID", Keywords = "open steam dlc store page buy app id", ToolTip = "Opens the store page of the DLC with this App ID in the Steam overlay with the DLC in the cart."))
	static FSteamResult OpenSteamDLCStorePageByAppId(const UObject* WorldContextObject, int32 AppId);
};
