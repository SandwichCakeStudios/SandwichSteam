// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Data/SteamAppDefinition.h"

#include "Core/SteamInputRules.h"
#include "Core/SteamPresenceRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	const FName SteamAppDefinitionType(TEXT("SteamAppDefinition"));

	template <typename TDef, typename TKey>
	const TDef* FindRow(const TArray<TDef>& Rows, const TMap<TKey, int32>& Lookup, const TKey& Key)
	{
		const int32* Index = Lookup.Find(Key);
		return (Index && Rows.IsValidIndex(*Index)) ? &Rows[*Index] : nullptr;
	}
}

FPrimaryAssetType USteamAppDefinition::GetAssetType()
{
	return FPrimaryAssetType(SteamAppDefinitionType);
}

FPrimaryAssetId USteamAppDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetAssetType(), GetFName());
}

void USteamAppDefinition::PostLoad()
{
	Super::PostLoad();
	InvalidateLookups();
}

const FSteamStatDef* USteamAppDefinition::FindStat(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(Stats, StatByTag, Tag) : nullptr;
}

const FSteamStatDef* USteamAppDefinition::FindStat(FName ApiName) const
{
	BuildLookups();
	return FindRow(Stats, StatByName, ApiName);
}

const FSteamAchievementDef* USteamAppDefinition::FindAchievement(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(Achievements, AchievementByTag, Tag) : nullptr;
}

const FSteamAchievementDef* USteamAppDefinition::FindAchievement(FName ApiName) const
{
	BuildLookups();
	return FindRow(Achievements, AchievementByName, ApiName);
}

const FSteamLeaderboardDef* USteamAppDefinition::FindLeaderboard(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(Leaderboards, LeaderboardByTag, Tag) : nullptr;
}

const FSteamLeaderboardDef* USteamAppDefinition::FindLeaderboard(FName Name) const
{
	BuildLookups();
	return FindRow(Leaderboards, LeaderboardByName, Name);
}

const FSteamPresenceDef* USteamAppDefinition::FindPresence(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(Presence, PresenceByTag, Tag) : nullptr;
}

const FSteamDLCDef* USteamAppDefinition::FindDLC(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(DLC, DLCByTag, Tag) : nullptr;
}

const FSteamSessionProfileDef* USteamAppDefinition::FindSessionProfile(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(Sessions, SessionByTag, Tag) : nullptr;
}

const FSteamInputActionSetDef* USteamAppDefinition::FindInputSet(const FGameplayTag& Tag) const
{
	BuildLookups();
	return Tag.IsValid() ? FindRow(InputSets, InputSetByTag, Tag) : nullptr;
}

const FSteamPresenceDef* USteamAppDefinition::FindPresenceByToken(const FString& Token) const
{
	return Token.IsEmpty() ? nullptr : Presence.FindByPredicate([&Token](const FSteamPresenceDef& Row) { return Row.Token.Equals(Token, ESearchCase::IgnoreCase); });
}

const FSteamDLCDef* USteamAppDefinition::FindDLCByAppId(int32 AppId) const
{
	return AppId > 0 ? DLC.FindByPredicate([AppId](const FSteamDLCDef& Row) { return Row.AppId == AppId; }) : nullptr;
}

const FSteamInputActionSetDef* USteamAppDefinition::FindInputSetByName(FName SteamSetName) const
{
	return SteamSetName.IsNone() ? nullptr : InputSets.FindByPredicate([SteamSetName](const FSteamInputActionSetDef& Row) { return Row.SteamSetName == SteamSetName; });
}

void USteamAppDefinition::BuildLookups() const
{
	if (bLookupsBuilt)
	{
		return;
	}

	StatByTag.Reset();
	StatByName.Reset();
	AchievementByTag.Reset();
	AchievementByName.Reset();
	LeaderboardByTag.Reset();
	LeaderboardByName.Reset();
	PresenceByTag.Reset();
	DLCByTag.Reset();
	SessionByTag.Reset();
	InputSetByTag.Reset();

	// First row wins on duplicates (IsDataValid reports them in the editor).
	for (int32 Index = 0; Index < Stats.Num(); ++Index)
	{
		StatByName.FindOrAdd(Stats[Index].ApiName, Index);
		if (Stats[Index].Tag.IsValid())
		{
			StatByTag.FindOrAdd(Stats[Index].Tag, Index);
		}
	}

	for (int32 Index = 0; Index < Achievements.Num(); ++Index)
	{
		AchievementByName.FindOrAdd(Achievements[Index].ApiName, Index);
		if (Achievements[Index].Tag.IsValid())
		{
			AchievementByTag.FindOrAdd(Achievements[Index].Tag, Index);
		}
	}

	for (int32 Index = 0; Index < Leaderboards.Num(); ++Index)
	{
		LeaderboardByName.FindOrAdd(Leaderboards[Index].Name, Index);
		if (Leaderboards[Index].Tag.IsValid())
		{
			LeaderboardByTag.FindOrAdd(Leaderboards[Index].Tag, Index);
		}
	}

	for (int32 Index = 0; Index < Presence.Num(); ++Index)
	{
		if (Presence[Index].Tag.IsValid())
		{
			PresenceByTag.FindOrAdd(Presence[Index].Tag, Index);
		}
	}

	for (int32 Index = 0; Index < DLC.Num(); ++Index)
	{
		if (DLC[Index].Tag.IsValid())
		{
			DLCByTag.FindOrAdd(DLC[Index].Tag, Index);
		}
	}

	for (int32 Index = 0; Index < Sessions.Num(); ++Index)
	{
		if (Sessions[Index].Tag.IsValid())
		{
			SessionByTag.FindOrAdd(Sessions[Index].Tag, Index);
		}
	}

	for (int32 Index = 0; Index < InputSets.Num(); ++Index)
	{
		if (InputSets[Index].Tag.IsValid())
		{
			InputSetByTag.FindOrAdd(InputSets[Index].Tag, Index);
		}
	}

	bLookupsBuilt = true;
}

void USteamAppDefinition::InvalidateLookups() const
{
	bLookupsBuilt = false;
}

#if WITH_EDITOR
void USteamAppDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	InvalidateLookups();
}

namespace
{
	/** Reports duplicate names and tags of one row kind. */
	template <typename TDef, typename TNameFn>
	void ValidateUniqueness(const TArray<TDef>& Rows, TNameFn GetName, const TCHAR* Kind, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		TSet<FName> Names;
		TSet<FGameplayTag> Tags;
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const TDef& Row = Rows[Index];
			const FName Name = GetName(Row);
			if (Name.IsNone())
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefEmptyName", "{0} row {1} has no API name."), FText::FromString(Kind), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
			}
			else if (Names.Contains(Name))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefDuplicateName", "Duplicate {0} name '{1}'."), FText::FromString(Kind), FText::FromName(Name)));
				Result = EDataValidationResult::Invalid;
			}
			Names.Add(Name);

			if (!Row.Tag.IsValid())
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefNoTag", "{0} '{1}' has no gameplay tag, so it can only be accessed by name."), FText::FromString(Kind), FText::FromName(Name)));
			}
			else if (Tags.Contains(Row.Tag))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefDuplicateTag", "Gameplay tag '{0}' is used by more than one {1}."), FText::FromName(Row.Tag.GetTagName()), FText::FromString(Kind)));
				Result = EDataValidationResult::Invalid;
			}
			Tags.Add(Row.Tag);
		}
	}

	/** Rich presence rows: token format, unique tags and tokens, and Steam's key limits. */
	void ValidatePresence(const TArray<FSteamPresenceDef>& Rows, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		TSet<FString> Tokens;
		TSet<FGameplayTag> Tags;
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FSteamPresenceDef& Row = Rows[Index];
			if (Row.Token.IsEmpty())
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceNoToken", "Presence row {0} has no token."), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			const FText TokenText = FText::FromString(Row.Token);
			if (!Row.Token.StartsWith(TEXT("#")))
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceHash", "Presence token '{0}' does not start with #. Steam only translates tokens that do."), TokenText));
			}

			if (Tokens.Contains(Row.Token))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceDuplicateToken", "Duplicate presence token '{0}'."), TokenText));
				Result = EDataValidationResult::Invalid;
			}
			Tokens.Add(Row.Token);

			if (SandwichSteam::Presence::CheckValue(Row.Token) != SandwichSteam::Presence::EIssue::None)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceTokenLong", "Presence token '{0}' is longer than {1} bytes."), TokenText, FText::AsNumber(SandwichSteam::Presence::MaxValueBytes)));
				Result = EDataValidationResult::Invalid;
			}

			if (!Row.Tag.IsValid())
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceNoTag", "Presence '{0}' has no gameplay tag, so it cannot be set by tag."), TokenText));
			}
			else if (Tags.Contains(Row.Tag))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceDuplicateTag", "Gameplay tag '{0}' is used by more than one presence row."), FText::FromName(Row.Tag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			Tags.Add(Row.Tag);

			// steam_display takes one of the keys.
			if (Row.ExtraKeys.Num() + 1 > SandwichSteam::Presence::MaxKeys)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceTooManyKeys", "Presence '{0}' uses {1} keys. Steam allows {2} including steam_display."),
					TokenText, FText::AsNumber(Row.ExtraKeys.Num()), FText::AsNumber(SandwichSteam::Presence::MaxKeys)));
				Result = EDataValidationResult::Invalid;
			}

			TSet<FName> Keys;
			for (const FName Key : Row.ExtraKeys)
			{
				const FString KeyString = Key.ToString();
				const SandwichSteam::Presence::EIssue Issue = SandwichSteam::Presence::CheckKey(Key.IsNone() ? FString() : KeyString);
				if (Issue != SandwichSteam::Presence::EIssue::None)
				{
					Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceBadKey", "Presence '{0}' has an invalid key '{1}': {2}."),
						TokenText, FText::FromString(KeyString), FText::FromString(SandwichSteam::Presence::DescribeIssue(Issue))));
					Result = EDataValidationResult::Invalid;
				}
				else if (SandwichSteam::Presence::IsReservedKey(KeyString))
				{
					Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceReservedKey", "Presence '{0}' lists the key '{1}', which Steam reads itself. Use Set Steam Presence Group / Connect String for it."),
						TokenText, FText::FromString(KeyString)));
				}

				if (Keys.Contains(Key))
				{
					Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefPresenceDuplicateKey", "Presence '{0}' lists the key '{1}' twice."), TokenText, FText::FromString(KeyString)));
				}
				Keys.Add(Key);
			}
		}
	}
}

namespace
{
	/** DLC rows: valid and unique App IDs, unique tags. */
	void ValidateDLC(const TArray<FSteamDLCDef>& Rows, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		TSet<int32> AppIds;
		TSet<FGameplayTag> Tags;
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FSteamDLCDef& Row = Rows[Index];
			if (Row.AppId <= 0)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefDlcNoAppId", "DLC row {0} has no App ID."), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			const FText AppIdText = FText::AsNumber(Row.AppId, &FNumberFormattingOptions::DefaultNoGrouping());
			if (AppIds.Contains(Row.AppId))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefDlcDuplicateAppId", "Duplicate DLC App ID {0}."), AppIdText));
				Result = EDataValidationResult::Invalid;
			}
			AppIds.Add(Row.AppId);

			if (!Row.Tag.IsValid())
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefDlcNoTag", "DLC {0} has no gameplay tag, so it can only be checked by App ID."), AppIdText));
			}
			else if (Tags.Contains(Row.Tag))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefDlcDuplicateTag", "Gameplay tag '{0}' is used by more than one DLC."), FText::FromName(Row.Tag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			Tags.Add(Row.Tag);
		}
	}
}

namespace
{
	/** Longest lobby data key Steam accepts (k_nMaxLobbyKeyLength). */
	constexpr int32 MaxSessionKeyBytes = 255;

	/**
	 * Keys the Sessions feature owns (profile tag, session name, kick markers). Duplicated here as literals because the
	 * core module cannot depend on the Sessions module (SandwichSteam::Sessions::IsReservedLobbyKey / NameKey / ProfileKey).
	 */
	bool IsReservedSessionKey(const FString& Key)
	{
		return Key.Equals(TEXT("OSTPROFILE"), ESearchCase::IgnoreCase)
			|| Key.Equals(TEXT("OSTNAME"), ESearchCase::IgnoreCase)
			|| Key.StartsWith(TEXT("kick_"), ESearchCase::IgnoreCase);
	}

	/** Checks one settings map of a session profile: empty / duplicate / reserved / too long keys. */
	void ValidateSessionSettingsMap(const TMap<FName, FString>& Settings, const FText& TagText, const TCHAR* MapName, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		for (const TPair<FName, FString>& Setting : Settings)
		{
			const FString Key = Setting.Key.ToString();
			if (Setting.Key.IsNone() || Key.IsEmpty())
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionEmptyKey", "Session profile '{0}' has a {1} entry without a key."), TagText, FText::FromString(MapName)));
				Result = EDataValidationResult::Invalid;
			}
			else if (FTCHARToUTF8(*Key).Length() > MaxSessionKeyBytes)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionLongKey", "Session profile '{0}' has a {1} key longer than {2} bytes."), TagText, FText::FromString(MapName), FText::AsNumber(MaxSessionKeyBytes)));
				Result = EDataValidationResult::Invalid;
			}
			else if (IsReservedSessionKey(Key))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionReservedKey", "Session profile '{0}' uses the key '{1}' in {2}, which is reserved by the plugin (OSTPROFILE, OSTNAME, kick_*)."), TagText, FText::FromString(Key), FText::FromString(MapName)));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	/** Session profiles: a tag is the only handle, so it is required and unique; slot range, visibility and settings keys are checked. */
	void ValidateSessions(const TArray<FSteamSessionProfileDef>& Rows, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		TSet<FGameplayTag> Tags;
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FSteamSessionProfileDef& Row = Rows[Index];
			if (!Row.Tag.IsValid())
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionNoTag", "Session profile {0} has no gameplay tag. Sessions are created by tag, so it cannot be used."), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
				continue;
			}

			const FText TagText = FText::FromName(Row.Tag.GetTagName());
			if (Tags.Contains(Row.Tag))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionDuplicateTag", "Gameplay tag '{0}' is used by more than one session profile."), TagText));
				Result = EDataValidationResult::Invalid;
			}
			Tags.Add(Row.Tag);

			if (Row.MaxPlayers < 1 || Row.MaxPlayers > 250)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionPlayers", "Session profile '{0}' has {1} player slots. Steam lobbies hold 1 to 250."), TagText, FText::AsNumber(Row.MaxPlayers)));
				Result = EDataValidationResult::Invalid;
			}

			const int32 ResolvedMin = FMath::Max(Row.MinPlayers, 1);
			const int32 ResolvedDefault = Row.DefaultPlayers > 0 ? Row.DefaultPlayers : Row.MaxPlayers;
			if (ResolvedMin > Row.MaxPlayers)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionMinAboveMax", "Session profile '{0}' has Min Players ({1}) greater than Max Players ({2})."), TagText, FText::AsNumber(ResolvedMin), FText::AsNumber(Row.MaxPlayers)));
				Result = EDataValidationResult::Invalid;
			}
			else if (ResolvedDefault < ResolvedMin || ResolvedDefault > Row.MaxPlayers)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionDefaultOutOfRange", "Session profile '{0}' has Default Players ({1}) outside Min/Max Players ({2}/{3})."), TagText, FText::AsNumber(ResolvedDefault), FText::AsNumber(ResolvedMin), FText::AsNumber(Row.MaxPlayers)));
				Result = EDataValidationResult::Invalid;
			}

			if (Row.AllowedVisibilities != 0)
			{
				const uint8 DefaultBit = static_cast<uint8>(1 << static_cast<uint8>(Row.Visibility));
				if ((Row.AllowedVisibilities & DefaultBit) == 0)
				{
					Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionVisibilityNotAllowed", "Session profile '{0}' has a Default Visibility that is not in Allowed Visibilities."), TagText));
					Result = EDataValidationResult::Invalid;
				}
			}

			if (!Row.bUsesPresence)
			{
				// Without presence the OSS creates a game server session, not a lobby: the plugin's search only lists lobbies.
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionNoPresence", "Session profile '{0}' has Uses Presence off: it is not found by Find Steam Sessions, and invites and Join Game do not work. Only for games that connect by their own address."), TagText));
			}

			ValidateSessionSettingsMap(Row.Settings, TagText, TEXT("Fixed Settings"), Context, Result);
			ValidateSessionSettingsMap(Row.PlayerSettings, TagText, TEXT("Player Settings"), Context, Result);

			for (const TPair<FName, FString>& Setting : Row.PlayerSettings)
			{
				if (Row.Settings.Contains(Setting.Key))
				{
					Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefSessionSettingOverlap", "Session profile '{0}' lists the key '{1}' in both Fixed Settings and Player Settings."), TagText, FText::FromName(Setting.Key)));
					Result = EDataValidationResult::Invalid;
				}
			}
		}
	}
}

namespace
{
	/** Steam Input rows: unique tags and set names, valid names, one kind per action name, a mapping context per set. */
	void ValidateInputSets(const TArray<FSteamInputActionSetDef>& Rows, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		TSet<FGameplayTag> Tags;
		TSet<FName> SetNames;
		TMap<FName, ESteamInputActionKind> Kinds;

		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FSteamInputActionSetDef& Row = Rows[Index];
			const FText RowText = Row.SteamSetName.IsNone() ? FText::AsNumber(Index) : FText::FromName(Row.SteamSetName);

			if (!Row.Tag.IsValid())
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputNoTag", "Steam Input set '{0}' has no gameplay tag. Sets are switched by tag, so it cannot be used."), RowText));
				Result = EDataValidationResult::Invalid;
			}
			else if (Tags.Contains(Row.Tag))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputDuplicateTag", "Gameplay tag '{0}' is used by more than one Steam Input set."), FText::FromName(Row.Tag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
			Tags.Add(Row.Tag);

			const SandwichSteam::Input::ENameIssue SetIssue = SandwichSteam::Input::CheckName(Row.SteamSetName.IsNone() ? FString() : Row.SteamSetName.ToString(), /*bIsAction*/ false);
			if (SetIssue != SandwichSteam::Input::ENameIssue::None)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputSetName", "Steam Input set '{0}' has an invalid name: {1}."), RowText, FText::FromString(SandwichSteam::Input::DescribeIssue(SetIssue))));
				Result = EDataValidationResult::Invalid;
			}
			else if (SetNames.Contains(Row.SteamSetName))
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputDuplicateSet", "Duplicate Steam Input set name '{0}'."), RowText));
				Result = EDataValidationResult::Invalid;
			}
			SetNames.Add(Row.SteamSetName);

			if (Row.InputContext.IsNull())
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputNoContext", "Steam Input set '{0}' has no mapping context, so its keys are not mapped to your Input Actions unless your own context does it."), RowText));
			}

			if (Row.Actions.IsEmpty())
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputNoActions", "Steam Input set '{0}' has no actions."), RowText));
			}

			TSet<FName> ActionNames;
			for (const FSteamInputActionDef& Action : Row.Actions)
			{
				const FString ActionName = Action.SteamName.IsNone() ? FString() : Action.SteamName.ToString();
				const SandwichSteam::Input::ENameIssue ActionIssue = SandwichSteam::Input::CheckName(ActionName, /*bIsAction*/ true);
				if (ActionIssue != SandwichSteam::Input::ENameIssue::None)
				{
					Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputActionName", "Steam Input set '{0}' has an action with an invalid name '{1}': {2}."),
						RowText, FText::FromString(ActionName), FText::FromString(SandwichSteam::Input::DescribeIssue(ActionIssue))));
					Result = EDataValidationResult::Invalid;
					continue;
				}

				if (ActionNames.Contains(Action.SteamName))
				{
					Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputDuplicateAction", "Steam Input set '{0}' lists the action '{1}' twice."), RowText, FText::FromString(ActionName)));
					Result = EDataValidationResult::Invalid;
				}
				ActionNames.Add(Action.SteamName);

				// The key SteamInput_<Name> is shared by every set, so an action name means one kind everywhere.
				if (const ESteamInputActionKind* Known = Kinds.Find(Action.SteamName))
				{
					if (*Known != Action.Kind)
					{
						Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefInputKindMismatch", "Steam Input action '{0}' has different kinds in different sets. The key SteamInput_{0} is shared, so use one kind."), FText::FromString(ActionName)));
						Result = EDataValidationResult::Invalid;
					}
				}
				else
				{
					Kinds.Add(Action.SteamName, Action.Kind);
				}
			}
		}
	}
}

EDataValidationResult USteamAppDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateUniqueness(Stats, [](const FSteamStatDef& Row) { return Row.ApiName; }, TEXT("stat"), Context, Result);
	ValidateUniqueness(Achievements, [](const FSteamAchievementDef& Row) { return Row.ApiName; }, TEXT("achievement"), Context, Result);
	ValidateUniqueness(Leaderboards, [](const FSteamLeaderboardDef& Row) { return Row.Name; }, TEXT("leaderboard"), Context, Result);
	ValidatePresence(Presence, Context, Result);
	ValidateDLC(DLC, Context, Result);
	ValidateSessions(Sessions, Context, Result);
	ValidateInputSets(InputSets, Context, Result);

	for (const FSteamStatDef& Stat : Stats)
	{
		if (Stat.bClampToRange)
		{
			if (Stat.Min > Stat.Max)
			{
				Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefStatRange", "Stat '{0}' has Min greater than Max."), FText::FromName(Stat.ApiName)));
				Result = EDataValidationResult::Invalid;
			}
			else if (Stat.Default < Stat.Min || Stat.Default > Stat.Max)
			{
				Context.AddWarning(FText::Format(NSLOCTEXT("SandwichSteam", "DefStatDefault", "Stat '{0}' has a default value outside its Min/Max range."), FText::FromName(Stat.ApiName)));
			}
		}
	}

	for (const FSteamAchievementDef& Achievement : Achievements)
	{
		if (Achievement.ProgressStat.IsNone())
		{
			continue;
		}

		if (!FindStat(Achievement.ProgressStat))
		{
			Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefProgressStatMissing", "Achievement '{0}' uses progress stat '{1}', which is not in the Stats list."),
				FText::FromName(Achievement.ApiName), FText::FromName(Achievement.ProgressStat)));
			Result = EDataValidationResult::Invalid;
		}

		if (Achievement.ProgressMax <= 0)
		{
			Context.AddError(FText::Format(NSLOCTEXT("SandwichSteam", "DefProgressMax", "Achievement '{0}' has a progress stat but Progress Max is not greater than 0."), FText::FromName(Achievement.ApiName)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif // WITH_EDITOR
