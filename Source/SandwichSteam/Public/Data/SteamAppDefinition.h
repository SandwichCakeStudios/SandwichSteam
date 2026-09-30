// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SteamAppDefinition.generated.h"

class UTexture2D;

/** How a Steam stat stores its value. Must match the stat type configured in Steamworks. */
UENUM(BlueprintType)
enum class ESteamStatType : uint8
{
	/** 32-bit integer stat. */
	Int,
	/** 32-bit float stat. */
	Float,
	/** Running average per session (UpdateAvgRate). Stored as a float. */
	AvgRate
};

/** Leaderboard sort order. */
UENUM(BlueprintType)
enum class ESteamLeaderboardSortMethod : uint8
{
	/** Lower scores rank higher (times). */
	Ascending,
	/** Higher scores rank higher (points). */
	Descending
};

/** How the Steam website shows leaderboard scores. */
UENUM(BlueprintType)
enum class ESteamLeaderboardDisplayType : uint8
{
	Numeric,
	TimeSeconds,
	TimeMilliSeconds
};

/** One stat of the game, as configured in Steamworks. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamStatDef
{
	GENERATED_BODY()

	/** API Name of the stat in Steamworks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "API Name of the stat exactly as configured in Steamworks."))
	FName ApiName;

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Stat", ToolTip = "Gameplay tag used to access this stat from Blueprints and C++. Add tags under Steam.Stat in the Gameplay Tags settings."))
	FGameplayTag Tag;

	/** Storage type of the stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Storage type of the stat. Must match the type configured in Steamworks."))
	ESteamStatType Type = ESteamStatType::Int;

	/** Clamp values written through the plugin to [Min, Max]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "When enabled, values written through the plugin are clamped to the Min/Max range."))
	bool bClampToRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (EditCondition = "bClampToRange", ToolTip = "Smallest value the plugin writes."))
	double Min = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (EditCondition = "bClampToRange", ToolTip = "Largest value the plugin writes."))
	double Max = 0.0;

	/** Default value configured in Steamworks (documentation and validation only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Default value configured in Steamworks. Used only for validation."))
	double Default = 0.0;
};

/** One achievement of the game, as configured in Steamworks. Display name, description and icon come from Steam at runtime. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamAchievementDef
{
	GENERATED_BODY()

	/** API Name of the achievement in Steamworks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "API Name of the achievement exactly as configured in Steamworks."))
	FName ApiName;

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Achievement", ToolTip = "Gameplay tag used to access this achievement from Blueprints and C++. Add tags under Steam.Achievement in the Gameplay Tags settings."))
	FGameplayTag Tag;

	/** Stat (API Name) that counts towards this achievement. Empty for achievements without progress. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "API Name of the stat that measures progress towards this achievement. When set, changing that stat shows the Steam progress toast. Leave empty for all-or-nothing achievements."))
	FName ProgressStat;

	/** Stat value at which the achievement completes. Needed when ProgressStat is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ClampMin = "0", ToolTip = "Value of the progress stat at which the achievement is complete. Must be greater than 0 when a progress stat is set."))
	int32 ProgressMax = 0;

	/** Hidden in Steamworks until unlocked (documentation only, Steam decides at runtime). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Mirrors the Hidden flag from Steamworks. Documentation only: Steam decides what players see."))
	bool bHidden = false;

#if WITH_EDITORONLY_DATA
	/** Name shown in editor lists only. The player sees the localized name from Steam. */
	UPROPERTY(EditAnywhere, Category = "Steam|Editor", meta = (ToolTip = "Name shown in editor lists only. Players see the localized name from Steam."))
	FString EditorDisplayName;

	/** Icon shown in editor lists only. */
	UPROPERTY(EditAnywhere, Category = "Steam|Editor", meta = (ToolTip = "Icon shown in editor lists only. Players see the icon from Steam."))
	TSoftObjectPtr<UTexture2D> EditorIcon;
#endif
};

/** One leaderboard of the game. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamLeaderboardDef
{
	GENERATED_BODY()

	/** Leaderboard name in Steamworks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Name of the leaderboard exactly as configured in Steamworks."))
	FName Name;

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Leaderboard", ToolTip = "Gameplay tag used to access this leaderboard from Blueprints and C++. Add tags under Steam.Leaderboard in the Gameplay Tags settings."))
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (EditCondition = "bCreateIfMissing", ToolTip = "Sort order used when the leaderboard is created by the game."))
	ESteamLeaderboardSortMethod SortMethod = ESteamLeaderboardSortMethod::Descending;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (EditCondition = "bCreateIfMissing", ToolTip = "Display type used when the leaderboard is created by the game."))
	ESteamLeaderboardDisplayType DisplayType = ESteamLeaderboardDisplayType::Numeric;

	/** Create the leaderboard on first use when Steam does not know it yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Create the leaderboard when it does not exist yet. Only works for leaderboards not managed in Steamworks."))
	bool bCreateIfMissing = false;
};

/**
 * One rich presence status of the game (what friends see next to the game name). Set at runtime with the tag,
 * for example Steam.Presence.InMatch with the argument map=Harbor.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamPresenceDef
{
	GENERATED_BODY()

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Presence", ToolTip = "Gameplay tag used to set this status from Blueprints and C++. Add tags under Steam.Presence in the Gameplay Tags settings."))
	FGameplayTag Tag;

	/** Localization token, as used in the rich presence localization file uploaded to Steamworks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Localization token of this status, for example #Status_InMatch. It has to start with #. Steam shows the text the token has in the localization file you upload to Steamworks."))
	FString Token;

	/** Keys the text of the token substitutes as %key%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Keys the text of this token uses as {key} (written %key% in the Steam file), for example map. They are the arguments of Set Steam Presence."))
	TArray<FName> ExtraKeys;

#if WITH_EDITORONLY_DATA
	/** Text of the status. Localize it like any FText: the localization generator writes one language block per Steam language. */
	UPROPERTY(EditAnywhere, Category = "Steam|Editor", meta = (ToolTip = "Text friends see, for example \"Playing on {map}\". Write the keys in braces. Localize it like any text: Generate Rich Presence Localization writes one block per Steam language."))
	FText LocalizedText;
#endif
};

/** One DLC of the game. Ownership and installation come from Steam at runtime. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamDLCDef
{
	GENERATED_BODY()

	/** Steam App ID of the DLC. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ClampMin = "1", ToolTip = "Steam App ID of the DLC, as shown in Steamworks."))
	int32 AppId = 0;

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.DLC", ToolTip = "Gameplay tag used to check this DLC from Blueprints and C++. Add tags under Steam.DLC in the Gameplay Tags settings."))
	FGameplayTag Tag;

#if WITH_EDITORONLY_DATA
	/** Name shown in editor lists only. Players see the name from Steam. */
	UPROPERTY(EditAnywhere, Category = "Steam|Editor", meta = (ToolTip = "Name shown in editor lists only. Players see the name from Steam."))
	FString EditorName;
#endif
};

/** Who can find and join a session. */
UENUM(BlueprintType)
enum class ESteamSessionVisibility : uint8
{
	/** Listed in searches. Anyone can join. */
	Public,
	/** Not listed. Friends can join from the friends list and through invites. */
	FriendsOnly,
	/** Not listed. Only invited players can join. */
	Private
};

/**
 * One kind of match the game can host (for example Coop or Ranked). Created at runtime by tag.
 * A session made from a profile can also be created with player-chosen settings (Create Steam Session From Profile): the
 * fields below are presets the host menu can edit, plus optional guardrails on what the player may choose. Every guardrail
 * is optional; an empty one (0, or an empty map) means no restriction. Guardrails are checked on the host only: a modified
 * client can skip them, the same as any client-hosted Steam lobby.
 */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamSessionProfileDef
{
	GENERATED_BODY()

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Session", ToolTip = "Gameplay tag used to create this kind of session from Blueprints and C++. Add tags under Steam.Session in the Gameplay Tags settings."))
	FGameplayTag Tag;

	/** Player slots, including the host. Upper limit: the player may pick fewer, never more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (DisplayName = "Max Players (limit)", ClampMin = "1", ClampMax = "250", ToolTip = "Upper limit of player slots, including the host. A Steam lobby holds at most 250 members. The player's requested value is clamped to Min/Max Players; the applied value is reported back."))
	int32 MaxPlayers = 4;

	/** Lower limit of player slots. 0 means 1 (no real limit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ClampMin = "0", ClampMax = "250", ToolTip = "Lower limit of player slots the player may choose. 0 means 1 (no real limit)."))
	int32 MinPlayers = 0;

	/** Preset shown in the host menu. 0 means Max Players. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ClampMin = "0", ClampMax = "250", ToolTip = "Preset player count for the host menu. 0 means Max Players."))
	int32 DefaultPlayers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (DisplayName = "Default Visibility", ToolTip = "Public sessions are listed in searches. Friends only sessions can be joined from the friends list and invites. Private sessions only by invite. Preset for the host menu."))
	ESteamSessionVisibility Visibility = ESteamSessionVisibility::Public;

	/** Visibilities the player may pick. 0 means every visibility is allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Bitmask, BitmaskEnum = "/Script/SandwichSteam.ESteamSessionVisibility", ToolTip = "Visibilities the player may choose. Empty (no box checked) means every visibility is allowed. Default Visibility must be one of the allowed ones."))
	uint8 AllowedVisibilities = 0;

	/** Create a Steam lobby with presence, so friends see the game as joinable and invites work. Designer only: ignored in the player-chosen settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Creates the session as a Steam lobby that friends can see and join. Turn it off only for sessions that are not meant to be joined through Steam. Designer only: the player cannot change it, the applied value is reported back."))
	bool bUsesPresence = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Default for allow players to join after the match has started."))
	bool bAllowJoinInProgress = true;

	/** When false, the player's requested value must match bAllowJoinInProgress or the request fails. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Let the player change Allow Join In Progress. When off, a request with a different value fails."))
	bool bPlayerCanChangeJoinInProgress = true;

	/** Extra key / value pairs always set, the player cannot override them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (DisplayName = "Fixed Settings", ToolTip = "Extra key and value pairs always set with the session; the player cannot override them. Searches can filter on them. Keys are at most 255 bytes."))
	TMap<FName, FString> Settings;

	/** Keys the player may set, with their default values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Keys the player may set, with their default values used until the player picks something else. Must not overlap Fixed Settings."))
	TMap<FName, FString> PlayerSettings;

	/** When true, the player may add setting keys that are in neither map above. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Let the player add setting keys that are in neither Fixed Settings nor Player Settings. When off, an unknown key fails the request."))
	bool bAllowExtraSettings = true;

	/** Longest display name the player may pick. 0 means 64 characters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ClampMin = "0", ToolTip = "Longest session name the player may pick. 0 means 64 characters. Longer names are trimmed, not refused."))
	int32 MaxNameLength = 0;
};

/** What kind of value a Steam Input action delivers. */
UENUM(BlueprintType)
enum class ESteamInputActionKind : uint8
{
	/** On / off (a button). Becomes the key SteamInput_<Name>. */
	Button,
	/** One analog value 0 to 1 (a trigger). Becomes the axis key SteamInput_<Name>. */
	Trigger,
	/** Two analog values (a stick, trackpad or gyro). Becomes the 2D key SteamInput_<Name> made of SteamInput_<Name>_X and _Y. */
	StickPad
};

/** One Steam Input action of an action set. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamInputActionDef
{
	GENERATED_BODY()

	/** Action name in the Steam Input action file (game_actions_<AppId>.vdf). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Action name as it is in the Steam Input action file, for example Jump. Letters, digits and underscores; must not end in _X or _Y. The game receives the key SteamInput_<Name>."))
	FName SteamName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Button: on / off. Trigger: one analog value. Stick / Pad: two analog values."))
	ESteamInputActionKind Kind = ESteamInputActionKind::Button;

	/** How Steam turns the stick, pad or gyro into values. Written into the generated action file. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (EditCondition = "Kind == ESteamInputActionKind::StickPad", EditConditionHides, ToolTip = "Steam input mode of the stick / pad, for example joystick_move, joystick_camera, absolute_mouse, relative_mouse, dpad. Only used by the generated action file."))
	FName InputMode = TEXT("joystick_move");

#if WITH_EDITORONLY_DATA
	/** Name shown in the Steam Input configurator and in the generated localization. */
	UPROPERTY(EditAnywhere, Category = "Steam|Editor", meta = (ToolTip = "Text the Steam Input configurator shows for this action (generated action file, English). Empty = the action name."))
	FString EditorDisplayName;
#endif
};

/** One Steam Input action set (or action set layer) and the Enhanced Input mapping context that goes with it. */
USTRUCT(BlueprintType)
struct SANDWICHSTEAM_API FSteamInputActionSetDef
{
	GENERATED_BODY()

	/** Game side handle used by the Blueprint and C++ API. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (Categories = "Steam.Input.ActionSet", ToolTip = "Gameplay tag used to switch to this action set from Blueprints and C++. Add tags under Steam.Input.ActionSet in the Gameplay Tags settings."))
	FGameplayTag Tag;

	/** Name of the set in the Steam Input action file. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Name of the action set in the Steam Input action file, for example Gameplay or Menu. Letters, digits and underscores."))
	FName SteamSetName;

	/** A layer adds its actions on top of the active set instead of replacing it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "A layer is switched on and off on top of the active action set (Activate / Deactivate Steam Input Layer). A normal set replaces the previous one."))
	bool bLayer = false;

	/** Enhanced Input mapping context that maps the SteamInput_ keys of this set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (AllowedClasses = "/Script/EnhancedInput.InputMappingContext", ToolTip = "Enhanced Input mapping context that maps the Steam Input keys of this set to your Input Actions. It is added to the local player while the set is active and removed when the set changes."))
	TSoftObjectPtr<UObject> InputContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (ToolTip = "Priority of the mapping context on the local player."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "SteamName", ToolTip = "Actions of this set."))
	TArray<FSteamInputActionDef> Actions;

#if WITH_EDITORONLY_DATA
	/** Name shown in the Steam Input configurator. */
	UPROPERTY(EditAnywhere, Category = "Steam|Editor", meta = (ToolTip = "Text the Steam Input configurator shows for this set (generated action file, English). Empty = the set name."))
	FString EditorDisplayName;
#endif
};

/**
 * The single data asset that describes the Steam side of the game: stats, achievements, leaderboards, rich presence, DLC, session profiles and Steam Input action sets.
 * Assign it in Project Settings > Plugins > Sandwich Steam. Feature modules read it at startup.
 * Rows are keyed by ApiName (Steam's truth) and Tag (the handle the game uses).
 */
UCLASS(BlueprintType, meta = (DisplayName = "Steam App Definition"))
class SANDWICHSTEAM_API USteamAppDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Primary asset type of every definition asset. */
	static FPrimaryAssetType GetAssetType();

	//~ Begin UObject
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject

	/** Lookups (game thread). Return nullptr when the row does not exist. The pointers stay valid until the asset is edited. */
	const FSteamStatDef* FindStat(const FGameplayTag& Tag) const;
	const FSteamStatDef* FindStat(FName ApiName) const;
	const FSteamAchievementDef* FindAchievement(const FGameplayTag& Tag) const;
	const FSteamAchievementDef* FindAchievement(FName ApiName) const;
	const FSteamLeaderboardDef* FindLeaderboard(const FGameplayTag& Tag) const;
	const FSteamLeaderboardDef* FindLeaderboard(FName Name) const;
	const FSteamPresenceDef* FindPresence(const FGameplayTag& Tag) const;
	const FSteamDLCDef* FindDLC(const FGameplayTag& Tag) const;
	const FSteamSessionProfileDef* FindSessionProfile(const FGameplayTag& Tag) const;
	const FSteamInputActionSetDef* FindInputSet(const FGameplayTag& Tag) const;

	/** Lookups by Steam's own identifier, for the by-name / by-App-ID APIs. Linear: these arrays are short. */
	const FSteamPresenceDef* FindPresenceByToken(const FString& Token) const;
	const FSteamDLCDef* FindDLCByAppId(int32 AppId) const;
	const FSteamInputActionSetDef* FindInputSetByName(FName SteamSetName) const;

	/** Stats Steam knows about for this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "ApiName", ToolTip = "Stats of the game as configured in Steamworks."))
	TArray<FSteamStatDef> Stats;

	/** Achievements Steam knows about for this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "ApiName", ToolTip = "Achievements of the game as configured in Steamworks."))
	TArray<FSteamAchievementDef> Achievements;

	/** Leaderboards of this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "Name", ToolTip = "Leaderboards of the game."))
	TArray<FSteamLeaderboardDef> Leaderboards;

	/** Rich presence statuses of this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "Token", ToolTip = "Rich presence statuses of the game (what friends see next to the game name)."))
	TArray<FSteamPresenceDef> Presence;

	/** DLC of this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "AppId", ToolTip = "DLC of the game. Check them by gameplay tag with the DLC feature."))
	TArray<FSteamDLCDef> DLC;

	/** Session profiles of this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "Tag", ToolTip = "Kinds of sessions the game can host. Create one by gameplay tag with the Sessions feature."))
	TArray<FSteamSessionProfileDef> Sessions;

	/** Steam Input action sets and layers of this game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Steam", meta = (TitleProperty = "SteamSetName", ToolTip = "Steam Input action sets and layers. Switch between them by gameplay tag with the Steam Input feature; each one can bring its own Enhanced Input mapping context."))
	TArray<FSteamInputActionSetDef> InputSets;

private:
	void BuildLookups() const;
	void InvalidateLookups() const;

	mutable bool bLookupsBuilt = false;
	mutable TMap<FGameplayTag, int32> StatByTag;
	mutable TMap<FName, int32> StatByName;
	mutable TMap<FGameplayTag, int32> AchievementByTag;
	mutable TMap<FName, int32> AchievementByName;
	mutable TMap<FGameplayTag, int32> LeaderboardByTag;
	mutable TMap<FName, int32> LeaderboardByName;
	mutable TMap<FGameplayTag, int32> PresenceByTag;
	mutable TMap<FGameplayTag, int32> DLCByTag;
	mutable TMap<FGameplayTag, int32> SessionByTag;
	mutable TMap<FGameplayTag, int32> InputSetByTag;
};
