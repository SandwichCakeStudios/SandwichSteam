// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SteamFeatureSubsystem.h"
#include "Data/SteamAppDefinition.h"
#include "SteamDLCTypes.h"
#include "SteamDLCSubsystem.generated.h"

class FSteamDLCBackend;

/**
 * Steam DLC (ISteamApps). Client only.
 *
 * DLC are rows of the Steam App Definition (App ID + gameplay tag), so gameplay checks by tag (IsOwned(Steam.DLC.Soundtrack))
 * survive an App ID change. Everything can also be asked by App ID. Steam answers ownership and installation from memory,
 * so the checks are cheap and are not cached here: they always reflect what Steam says now.
 * Installing and uninstalling only ask Steam to do it; OnDLCInstalled fires when an install finished.
 */
UCLASS()
class SANDWICHSTEAMDLC_API USteamDLCSubsystem : public USteamFeatureSubsystem
{
	GENERATED_BODY()

public:
	/** DLC subsystem of the GameInstance that owns WorldContext, or nullptr. */
	static USteamDLCSubsystem* Get(const UObject* WorldContext);

	USteamDLCSubsystem();
	virtual ~USteamDLCSubsystem() override;

	//~ Begin USteamFeatureSubsystem
	virtual FGameplayTag GetFeatureTag() const override;
	virtual ESteamFeatureScope GetFeatureScope() const override { return ESteamFeatureScope::ClientOnly; }
	//~ End USteamFeatureSubsystem

	/** True when the local user owns the DLC. False for unknown tags and while inactive. */
	bool IsOwned(const FGameplayTag& DlcTag) const;
	bool IsOwnedApp(int32 AppId) const;

	/** True when the DLC files are installed. */
	bool IsInstalled(const FGameplayTag& DlcTag) const;
	bool IsInstalledApp(int32 AppId) const;

	/** Every DLC Steam knows for this game, joined with the tags of the App Definition. */
	TArray<FSteamDLCInfo> ListDLC() const;

	/** Asks Steam to install / uninstall an owned DLC. OnDLCInstalled fires when the install is done. */
	FSteamResult Install(const FGameplayTag& DlcTag);
	FSteamResult InstallApp(int32 AppId);
	FSteamResult Uninstall(const FGameplayTag& DlcTag);
	FSteamResult UninstallApp(int32 AppId);

	/** Download state of a DLC. bDownloading is false when nothing is downloading. */
	FSteamDLCProgress GetDownloadProgress(const FGameplayTag& DlcTag) const;
	FSteamDLCProgress GetDownloadProgressApp(int32 AppId) const;

	/** Opens the store page of the DLC in the overlay, with the DLC added to the cart. */
	FSteamResult OpenStorePage(const FGameplayTag& DlcTag);
	FSteamResult OpenStorePageApp(int32 AppId);

	/** The App ID of a DLC tag, or 0 when the tag is not a row of the App Definition. */
	int32 ResolveAppId(const FGameplayTag& DlcTag) const;

	/** The App Definition the tags are resolved with. Null when none is assigned (then only App IDs work). */
	const USteamAppDefinition* GetDefinition() const { return Definition; }

	/** Called when a DLC finished installing. */
	UPROPERTY(BlueprintAssignable, Category = "Steam|DLC", meta = (ToolTip = "Called when a DLC finished installing. Tag is empty for DLC without a row in the Steam App Definition."))
	FOnSteamDLCInstalled OnDLCInstalled;

#if SANDWICHSTEAM_WITH_DEBUG
	/** Multi-line report for Steam.DLC.Dump. */
	FString BuildDebugString() const;
#endif

protected:
	//~ Begin USteamFeatureSubsystem
	virtual bool InitializeFeature() override;
	virtual void ShutdownFeature() override;
	//~ End USteamFeatureSubsystem

private:
	friend class FSteamDLCBackend;

	/** Game thread, called through the dispatcher by the backend. */
	void HandleDlcInstalled(int32 AppId);

	FSteamResult ValidateAppId(int32 AppId) const;
	FGameplayTag FindTag(int32 AppId) const;

	UPROPERTY(Transient)
	TObjectPtr<USteamAppDefinition> Definition;

	TSharedPtr<FSteamDLCBackend> Backend;
};
