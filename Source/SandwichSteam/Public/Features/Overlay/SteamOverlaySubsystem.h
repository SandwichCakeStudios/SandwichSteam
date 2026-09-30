// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamId.h"
#include "Features/Overlay/SteamOverlayTypes.h"
#include "SteamOverlaySubsystem.generated.h"

class FSteamOverlayBackend;

/**
 * Opens Steam overlay pages and reports when the overlay opens or closes (via the OSS ExternalUI delegate).
 * Optionally pauses the game while the overlay is open (USteamToolSettings::bAutoPauseOnOverlay).
 * Client only. Pages are opened with raw ISteamFriends calls, the same calls the OSS ExternalUI makes.
 */
UCLASS()
class SANDWICHSTEAM_API USteamOverlaySubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** Overlay subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamOverlaySubsystem* Get(const UObject* WorldContext);

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** True when the Steam overlay can be shown (the game was started through Steam and the overlay is not disabled). */
	UFUNCTION(BlueprintPure, Category = "Steam|Overlay", meta = (ToolTip = "True when the Steam overlay is available: the game was started through Steam and the overlay is enabled."))
	bool IsOverlayEnabled() const;

	/** True while the overlay is open. */
	UFUNCTION(BlueprintPure, Category = "Steam|Overlay", meta = (ToolTip = "True while the Steam overlay is open."))
	bool IsOverlayActive() const { return bOverlayActive; }

	/**
	 * Opens an overlay page that needs no argument (Friends, Community, Players, Settings, OfficialGameGroup, Stats, Achievements,
	 * Store = this game's store page). Other dialogs return Steam.Error.InvalidArgument; use the function that takes their argument.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (ToolTip = "Opens a Steam overlay page that needs no argument. Profile, chat, add friend and invite pages need Open Steam Overlay Target Dialog, web pages need Open Steam Overlay Web Page."))
	FSteamResult OpenDialog(ESteamOverlayDialog Dialog);

	/** Opens UserProfile, UserChat or AddFriend for a user, or InviteDialog for a lobby (Target = lobby ID). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (ToolTip = "Opens the profile, chat or add-friend page of a Steam user, or the invite dialog of a lobby (Target is then the lobby ID)."))
	FSteamResult OpenTargetDialog(ESteamOverlayDialog Dialog, FSteamId Target);

	/** Opens the store page of an app. AppId 0 = this game. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (ToolTip = "Opens the Steam store page of an app in the overlay. AppId 0 opens this game's page. Use it for DLC too."))
	FSteamResult OpenStore(int32 AppId = 0, ESteamOverlayStoreFlag Flag = ESteamOverlayStoreFlag::None);

	/** Opens a URL in the overlay browser. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Overlay", meta = (ToolTip = "Opens a web page in the Steam overlay browser. Modal pages close the overlay when the browser closes."))
	FSteamResult OpenWebPage(const FString& Url, bool bModal = false);

	/** Called when the overlay opens (true) or closes (false). */
	UPROPERTY(BlueprintAssignable, Category = "Steam|Overlay", meta = (ToolTip = "Called when the Steam overlay opens (true) or closes (false)."))
	FOnSteamOverlayActivated OnOverlayActivated;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.Overlay.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	/** Game thread. Applies the overlay state, the pause helper and broadcasts the event. */
	void HandleOverlayChanged(bool bActive);

	/** OSS ExternalUI delegate target. Marshals to the game thread when needed. */
	void HandleExternalUIChange(bool bIsOpening);

	bool ShouldAutoPause() const;
	void SetPausedByOverlay(bool bPause);
	FSteamResult RequireOverlayEnabled() const;

	TSharedPtr<FSteamOverlayBackend> Backend;
	FDelegateHandle ExternalUIHandle;
	bool bOverlayActive = false;
	bool bPausedByOverlay = false;
};
