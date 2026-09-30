// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Core/SteamId.h"
#include "Features/User/SteamUserTypes.h"
#include "SteamUserSubsystem.generated.h"

class FSteamAvatarCache;
class FSteamUserBackend;
class UTexture2D;
enum class ESteamAvatarFetch : uint8;

/**
 * The local Steam user: login state, persona name, Steam level, app ownership (incl. Family Sharing),
 * avatars of any user (lazy, cached, bounded) and Web API auth tickets.
 * Client only. Backend: raw ISteamUser / ISteamFriends / ISteamApps / ISteamUtils.
 */
UCLASS()
class SANDWICHSTEAM_API USteamUserSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** User subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamUserSubsystem* Get(const UObject* WorldContext);

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** True when the Steam client is logged on. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "True when the Steam client is logged on. False when Steam is not active."))
	bool IsLoggedOn() const;

	/** Steam ID of the local user. Invalid when Steam is not active. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "Steam ID of the local user. Invalid when Steam is not active."))
	FSteamId GetLocalSteamId() const;

	/** Display name of the local user. Empty when Steam is not active. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "Display (persona) name of the local user. Empty when Steam is not active."))
	FString GetPersonaName() const;

	/** Steam level of the local user. 0 when unknown. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "Steam level of the local user. 0 when unknown or Steam is not active."))
	int32 GetSteamLevel() const;

	/** True when the local user owns this game (any license, incl. Family Sharing). */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "True when the local user owns this game, including through Family Sharing."))
	bool IsSubscribed() const;

	/** True when the local user owns the given app (game or DLC). */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "True when the local user owns the given app or DLC."))
	bool IsSubscribedApp(int32 AppId) const;

	/** Steam ID of the account that owns the license. Differs from the local user for Family Sharing. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "Steam ID of the account that owns this game's license. It differs from the local user when the game is borrowed through Family Sharing."))
	FSteamId GetAppOwner() const;

	/** True when the game is borrowed through Family Sharing. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "True when the local user plays this game through Family Sharing (the license owner is someone else)."))
	bool IsFamilySharedLicense() const;

	/** When the user first purchased the given app (UTC). Returns the Unix epoch when never purchased. */
	UFUNCTION(BlueprintPure, Category = "Steam|User", meta = (ToolTip = "When the local user first purchased the given app (UTC). Returns 1970-01-01 when never purchased."))
	FDateTime GetEarliestPurchaseTime(int32 AppId) const;

	/** Avatar already in the cache, or null. Never starts a request. Use RequestAvatar or the Get Avatar node to load one. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User", meta = (ToolTip = "Returns the avatar if it is already cached, otherwise null. Never starts a request. Use the Get Steam Avatar node to load one."))
	UTexture2D* GetCachedAvatar(FSteamId UserId, ESteamAvatarSize Size = ESteamAvatarSize::Medium);

	/**
	 * Loads an avatar. OnComplete runs on the game thread, possibly before this returns (cache hit).
	 * Users without an avatar get USteamToolSettings::DefaultAvatar, or a NotSupported failure when none is set.
	 * Returns a failure (and never calls OnComplete) when the request could not be started.
	 */
	FSteamResult RequestAvatar(FSteamId UserId, ESteamAvatarSize Size, FSteamAvatarResultDelegate OnComplete);

	/**
	 * Requests a Web API auth ticket for the given service identity (Steam Web API "GetAuthTicketForWebApi").
	 * Needs a Steamworks SDK that has it, otherwise fails with Steam.Error.NotSupported.
	 * Outstanding tickets are cancelled when Steam shuts down.
	 */
	FSteamResult RequestWebApiTicket(const FString& Identity, FSteamWebApiTicketDelegate OnComplete);

	/** Invalidates a ticket obtained through RequestWebApiTicket. Call it once your backend consumed the ticket. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User", meta = (ToolTip = "Cancels a Web API ticket returned by the Get Steam Web API Ticket node. Call it after your backend has validated the ticket."))
	void CancelWebApiTicket(int32 TicketHandle);

	/** Called when the local user's display name changes. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|User", meta = (ToolTip = "Called when the local user's Steam display name changes."))
	FOnSteamPersonaNameChanged OnPersonaNameChanged;

	/** Called when the avatar of any user changes. Cached textures of that user were dropped; request the avatar again. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|User", meta = (ToolTip = "Called when a user's avatar changed. Cached textures of that user were dropped, request the avatar again."))
	FOnSteamAvatarChanged OnAvatarChanged;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.User.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamUserBackend;

	struct FPendingAvatar
	{
		FSteamId UserId;
		ESteamAvatarSize Size = ESteamAvatarSize::Medium;
		FSteamAvatarResultDelegate Callback;
	};

	struct FPendingTicket
	{
		uint32 Handle = 0;
		FSteamWebApiTicketDelegate Callback;
	};

	/** Game-thread handlers for the raw callbacks (invoked through the dispatcher). */
	void HandlePersonaStateChange(FSteamId UserId, bool bNameChanged, bool bAvatarChanged);
	void HandleAvatarImageLoaded(FSteamId UserId);
	void HandleWebApiTicketResponse(uint32 TicketHandle, int32 NativeResult, TArray<uint8> Ticket);

	ESteamAvatarFetch TryResolveAvatar(FSteamId UserId, ESteamAvatarSize Size, UTexture2D*& OutTexture);
	void ResolvePendingAvatars(FSteamId ChangedUser);
	void DeliverNoAvatar(const FSteamAvatarResultDelegate& Callback) const;

	TSharedPtr<FSteamUserBackend> Backend;
	TSharedPtr<FSteamAvatarCache> AvatarCache;

	TArray<FPendingAvatar> PendingAvatars;
	TArray<FPendingTicket> PendingTickets;
	TSet<uint32> IssuedTickets;
};
