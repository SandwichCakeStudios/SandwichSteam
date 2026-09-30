// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamAsyncActionBase.h"
#include "Core/SteamId.h"
#include "Features/User/SteamUserTypes.h"
#include "SteamUserAsyncActions.generated.h"

class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAvatarAsyncDelegate, UTexture2D*, Avatar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamWebApiTicketAsyncDelegate, const FString&, TicketHex, int32, TicketHandle);

/** Loads the avatar of any Steam user (downloads it from Steam when needed, cached afterwards). */
UCLASS()
class SANDWICHSTEAM_API USteamGetAvatarAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the loaded avatar. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|User", meta = (ToolTip = "Called with the avatar texture once it is loaded."))
	FSteamAvatarAsyncDelegate OnAvatarLoaded;

	/** Loads the avatar of a Steam user. Users without an avatar get the Default Avatar from the Sandwich Steam settings. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Get Steam Avatar", ToolTip = "Loads the avatar of a Steam user. Users without an avatar get the Default Avatar from the Sandwich Steam settings."))
	static USteamGetAvatarAsyncAction* GetSteamAvatar(const UObject* WorldContextObject, FSteamId UserId, ESteamAvatarSize Size = ESteamAvatarSize::Medium);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Avatar;

	FSteamId UserId;
	ESteamAvatarSize Size = ESteamAvatarSize::Medium;
};

/** Requests a Steam Web API auth ticket so your backend can verify who the player is. */
UCLASS()
class SANDWICHSTEAM_API USteamGetWebApiTicketAsyncAction : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Fires (in addition to OnSuccess) with the ticket. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|User", meta = (ToolTip = "Called with the ticket as a hex string and its handle. Send the hex string to your backend, then cancel the ticket with its handle."))
	FSteamWebApiTicketAsyncDelegate OnTicketReady;

	/** Requests a Web API ticket for the given service identity. Fails with NotSupported on Steamworks SDKs without this feature. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Get Steam Web API Ticket", ToolTip = "Requests a Steam Web API auth ticket for the given service identity. Send it to your backend and cancel it afterwards."))
	static USteamGetWebApiTicketAsyncAction* GetSteamWebApiTicket(const UObject* WorldContextObject, const FString& Identity);

protected:
	virtual TSubclassOf<USteamFeatureSubsystem> GetFeatureClass() const override;
	virtual void StartRequest() override;
	virtual void BroadcastSuccess() override;

private:
	FString Identity;
	FString TicketHex;
	int32 TicketHandle = 0;
};
