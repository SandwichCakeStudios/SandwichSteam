// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamSessionRules.h"

namespace SandwichSteam::Sessions
{
	FSessionShape MakeShape(ESteamSessionVisibility Visibility, int32 MaxPlayers, bool bUsesPresence, bool bAllowJoinInProgress)
	{
		FSessionShape Shape;
		const int32 Slots = FMath::Clamp(MaxPlayers, 1, 250);
		Shape.bUsesPresence = bUsesPresence;
		Shape.bUseLobbies = bUsesPresence;
		Shape.bAllowJoinInProgress = bAllowJoinInProgress;
		Shape.bAllowInvites = true;

		switch (Visibility)
		{
		case ESteamSessionVisibility::Public:
			Shape.PublicConnections = Slots;
			Shape.bShouldAdvertise = true;
			Shape.bAllowJoinViaPresence = bUsesPresence;
			break;
		case ESteamSessionVisibility::FriendsOnly:
			Shape.PublicConnections = Slots;
			Shape.bShouldAdvertise = false;
			Shape.bAllowJoinViaPresence = bUsesPresence;
			Shape.bAllowJoinViaPresenceFriendsOnly = bUsesPresence;
			break;
		case ESteamSessionVisibility::Private:
			Shape.PrivateConnections = Slots;
			Shape.bShouldAdvertise = false;
			Shape.bAllowJoinViaPresence = false;
			break;
		}

		return Shape;
	}

	ESteamJoinAction DecideJoin(ESteamJoinInMatchPolicy Policy, bool bInMatch, bool bAutoJoin, bool bAlreadyInTarget)
	{
		if (bAlreadyInTarget)
		{
			return ESteamJoinAction::Ignore;
		}

		if (!bInMatch)
		{
			return bAutoJoin ? ESteamJoinAction::JoinNow : ESteamJoinAction::AskGame;
		}

		switch (Policy)
		{
		case ESteamJoinInMatchPolicy::AutoLeave:
			return bAutoJoin ? ESteamJoinAction::LeaveAndJoin : ESteamJoinAction::AskGame;
		case ESteamJoinInMatchPolicy::AskGame:
			return ESteamJoinAction::AskGame;
		case ESteamJoinInMatchPolicy::Ignore:
		default:
			return ESteamJoinAction::Ignore;
		}
	}

	bool IsDuplicateRequest(double Now, double LastTime, const FSteamId& LastLobby, const FSteamId& Lobby, float WindowSeconds)
	{
		return Lobby.IsValid() && Lobby == LastLobby && WindowSeconds > 0.0f && (Now - LastTime) <= static_cast<double>(WindowSeconds);
	}

	ELostReason ClassifyNetworkFailure(ENetworkFailure::Type Failure)
	{
		switch (Failure)
		{
		case ENetworkFailure::ConnectionLost:
			return ELostReason::HostLeft;
		case ENetworkFailure::ConnectionTimeout:
			return ELostReason::Timeout;
		case ENetworkFailure::FailureReceived:
			return ELostReason::Kicked;
		default:
			return ELostReason::Failed;
		}
	}

	const TCHAR* LexToString(ELostReason Reason)
	{
		switch (Reason)
		{
		case ELostReason::HostLeft: return TEXT("HostLeft");
		case ELostReason::Timeout: return TEXT("Timeout");
		case ELostReason::Kicked: return TEXT("Kicked");
		default: return TEXT("Failed");
		}
	}

	bool ParseConnectString(const FString& Connect, FSteamJoinIntent& OutIntent)
	{
		OutIntent = FSteamJoinIntent();

		TArray<FString> Tokens;
		Connect.ParseIntoArrayWS(Tokens);
		for (int32 Index = 0; Index + 1 < Tokens.Num(); ++Index)
		{
			if (Tokens[Index].Equals(TEXT("+connect_lobby"), ESearchCase::IgnoreCase))
			{
				FSteamId LobbyId;
				if (FSteamId::FromString(Tokens[Index + 1], LobbyId))
				{
					OutIntent.Type = ESteamJoinIntentType::Lobby;
					OutIntent.LobbyId = LobbyId;
					return true;
				}
				return false;
			}

			if (Tokens[Index].Equals(TEXT("+connect"), ESearchCase::IgnoreCase))
			{
				OutIntent.Type = ESteamJoinIntentType::Server;
				OutIntent.ServerAddress = Tokens[Index + 1];
				return true;
			}
		}

		return false;
	}

	FString KickKey(const FSteamId& Member)
	{
		return KickKeyPrefix() + Member.ToString();
	}

	bool IsReservedLobbyKey(const FString& Key)
	{
		return Key.Equals(ProfileKey(), ESearchCase::IgnoreCase)
			|| Key.Equals(NameKey(), ESearchCase::IgnoreCase)
			|| Key.Equals(InGameKey(), ESearchCase::IgnoreCase)
			|| Key.Equals(MatchKey(), ESearchCase::IgnoreCase)
			|| Key.Equals(VisibilityKey(), ESearchCase::IgnoreCase)
			|| Key.Equals(PartyKey(), ESearchCase::IgnoreCase)
			|| Key.StartsWith(KickKeyPrefix(), ESearchCase::IgnoreCase);
	}

	bool ParseFlag(const FString& Value)
	{
		return Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	bool ValidateLobbyKey(const FString& Key, FText& OutError)
	{
		if (Key.IsEmpty())
		{
			OutError = NSLOCTEXT("SandwichSteam", "LobbyKeyEmpty", "The key is empty.");
			return false;
		}

		if (FTCHARToUTF8(*Key).Length() > MaxLobbyKeyBytes)
		{
			OutError = FText::Format(NSLOCTEXT("SandwichSteam", "LobbyKeyLong", "The key is longer than {0} bytes."), FText::AsNumber(MaxLobbyKeyBytes));
			return false;
		}

		if (IsReservedLobbyKey(Key))
		{
			OutError = FText::Format(NSLOCTEXT("SandwichSteam", "LobbyKeyReserved", "The key '{0}' is reserved by the plugin (OSTPROFILE, OSTNAME, OSTINGAME, OSTMATCH, OSTVISIBILITY, OSTPARTY, kick_*)."), FText::FromString(Key));
			return false;
		}

		return true;
	}

	bool ValidateLobbyValue(const FString& Value, FText& OutError)
	{
		if (FTCHARToUTF8(*Value).Length() > MaxLobbyValueBytes)
		{
			OutError = FText::Format(NSLOCTEXT("SandwichSteam", "LobbyValueLong", "The value is longer than {0} bytes."), FText::AsNumber(MaxLobbyValueBytes));
			return false;
		}
		return true;
	}

	bool ValidateChatText(const FString& Text, FText& OutError)
	{
		if (Text.IsEmpty())
		{
			OutError = NSLOCTEXT("SandwichSteam", "ChatEmpty", "The chat message is empty.");
			return false;
		}

		if (FTCHARToUTF8(*Text).Length() > MaxChatBytes)
		{
			OutError = FText::Format(NSLOCTEXT("SandwichSteam", "ChatLong", "The chat message is longer than {0} bytes."), FText::AsNumber(MaxChatBytes));
			return false;
		}
		return true;
	}

	ESteamLobbyMemberChange MemberChangeFromFlags(uint32 Flags)
	{
		// k_EChatMemberStateChangeEntered 0x1, Left 0x2, Disconnected 0x4, Kicked 0x8, Banned 0x10.
		if (Flags & 0x10) { return ESteamLobbyMemberChange::Banned; }
		if (Flags & 0x08) { return ESteamLobbyMemberChange::Kicked; }
		if (Flags & 0x04) { return ESteamLobbyMemberChange::Disconnected; }
		if (Flags & 0x02) { return ESteamLobbyMemberChange::Left; }
		return ESteamLobbyMemberChange::Entered;
	}

	ELobbyEnterOutcome ClassifyLobbyEnter(int32 Response)
	{
		switch (Response)
		{
		case 1: return ELobbyEnterOutcome::Success;
		case 2: return ELobbyEnterOutcome::NotFound;
		case 4: return ELobbyEnterOutcome::Full;
		case 3:  // not allowed
		case 6:  // banned
		case 7:  // limited account
		case 8:  // clan disabled
		case 9:  // community ban
		case 10: // member blocked you
		case 11: // you blocked a member
			return ELobbyEnterOutcome::Denied;
		default:
			return ELobbyEnterOutcome::Failed;
		}
	}

	// ---- Session settings (Phase 9c) ----

	namespace
	{
		const TCHAR* VisibilityName(ESteamSessionVisibility Visibility)
		{
			switch (Visibility)
			{
			case ESteamSessionVisibility::Public: return TEXT("Public");
			case ESteamSessionVisibility::FriendsOnly: return TEXT("Friends Only");
			default: return TEXT("Private");
			}
		}

		/** 1 << value, matching the Bitmask meta on FSteamSessionProfileDef::AllowedVisibilities. */
		uint8 VisibilityBit(ESteamSessionVisibility Visibility)
		{
			return static_cast<uint8>(1 << static_cast<uint8>(Visibility));
		}

		/** Validates one requested key/value and, on success, adds it to OutApplied. Rejects empty, too long and reserved keys. */
		bool ApplyRequestedSetting(const TPair<FName, FString>& Pair, FSteamSessionSettings& OutApplied, FText& OutError)
		{
			const FString Key = Pair.Key.ToString();
			if (!ValidateLobbyKey(Key, OutError) || !ValidateLobbyValue(Pair.Value, OutError))
			{
				return false;
			}
			OutApplied.Settings.Add(Pair.Key, Pair.Value);
			return true;
		}
	}

	bool ResolveSessionSettings(const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Requested, FSteamSessionSettings& OutApplied, FText& OutError)
	{
		OutApplied = FSteamSessionSettings();
		OutError = FText::GetEmpty();

		const int32 MaxNameLen = (Profile && Profile->MaxNameLength > 0) ? Profile->MaxNameLength : DefaultMaxNameLength;
		OutApplied.DisplayName = Requested.DisplayName.TrimStartAndEnd().Left(MaxNameLen);

		if (!Profile)
		{
			OutApplied.MaxPlayers = FMath::Clamp(Requested.MaxPlayers > 0 ? Requested.MaxPlayers : DefaultSessionPlayers, MinSessionPlayers, MaxSessionPlayers);
			OutApplied.Visibility = Requested.Visibility;
			OutApplied.bAllowJoinInProgress = Requested.bAllowJoinInProgress;
			OutApplied.bUsesPresence = Requested.bUsesPresence;

			for (const TPair<FName, FString>& Pair : Requested.Settings)
			{
				if (!ApplyRequestedSetting(Pair, OutApplied, OutError))
				{
					return false;
				}
			}
			return true;
		}

		const FSteamSessionProfileDef& P = *Profile;
		const int32 Min = FMath::Clamp(FMath::Max(P.MinPlayers, 1), MinSessionPlayers, P.MaxPlayers);
		const int32 Max = FMath::Clamp(P.MaxPlayers, Min, MaxSessionPlayers);
		const int32 Default = P.DefaultPlayers > 0 ? FMath::Clamp(P.DefaultPlayers, Min, Max) : Max;
		OutApplied.MaxPlayers = FMath::Clamp(Requested.MaxPlayers > 0 ? Requested.MaxPlayers : Default, Min, Max);

		if (P.AllowedVisibilities != 0 && (P.AllowedVisibilities & VisibilityBit(Requested.Visibility)) == 0)
		{
			OutError = FText::Format(NSLOCTEXT("SandwichSteam", "SessionVisibilityNotAllowed", "This session profile does not allow the visibility '{0}'."), FText::FromString(VisibilityName(Requested.Visibility)));
			return false;
		}
		OutApplied.Visibility = Requested.Visibility;

		if (!P.bPlayerCanChangeJoinInProgress && Requested.bAllowJoinInProgress != P.bAllowJoinInProgress)
		{
			OutError = NSLOCTEXT("SandwichSteam", "SessionJoinInProgressLocked", "This session profile does not let the player change Allow Join In Progress.");
			return false;
		}
		OutApplied.bAllowJoinInProgress = P.bPlayerCanChangeJoinInProgress ? Requested.bAllowJoinInProgress : P.bAllowJoinInProgress;

		OutApplied.bUsesPresence = P.bUsesPresence; // Designer only: the request is ignored.

		for (const TPair<FName, FString>& Pair : P.Settings)
		{
			OutApplied.Settings.Add(Pair.Key, Pair.Value);
		}
		for (const TPair<FName, FString>& Pair : P.PlayerSettings)
		{
			OutApplied.Settings.Add(Pair.Key, Pair.Value);
		}

		for (const TPair<FName, FString>& Pair : Requested.Settings)
		{
			const FString Key = Pair.Key.ToString();
			if (P.Settings.Contains(Pair.Key))
			{
				OutError = FText::Format(NSLOCTEXT("SandwichSteam", "SessionFixedSetting", "The setting '{0}' is fixed by this session profile and cannot be changed."), FText::FromString(Key));
				return false;
			}

			if (!P.PlayerSettings.Contains(Pair.Key) && !P.bAllowExtraSettings)
			{
				OutError = FText::Format(NSLOCTEXT("SandwichSteam", "SessionExtraSettingNotAllowed", "The setting '{0}' is not allowed by this session profile."), FText::FromString(Key));
				return false;
			}

			if (!ApplyRequestedSetting(Pair, OutApplied, OutError))
			{
				return false;
			}
		}

		return true;
	}

	void MakeDefaultSettings(const FSteamSessionProfileDef& Profile, FSteamSessionSettings& OutSettings)
	{
		// The profile's own defaults, resolved through the same rules a request goes through (they always succeed: the
		// profile's own Visibility and Allow Join In Progress are its own default).
		FSteamSessionSettings Requested;
		Requested.Visibility = Profile.Visibility;
		Requested.bAllowJoinInProgress = Profile.bAllowJoinInProgress;

		FText Error;
		ResolveSessionSettings(&Profile, Requested, OutSettings, Error);
	}

	bool ValidateSessionUpdate(const FSteamSessionProfileDef* Profile, const FSteamSessionSettings& Current, const FSteamSessionSettings& Requested, int32 CurrentPlayers, FSteamSessionSettings& OutApplied, FText& OutError)
	{
		if (!ResolveSessionSettings(Profile, Requested, OutApplied, OutError))
		{
			return false;
		}

		if (OutApplied.MaxPlayers < CurrentPlayers)
		{
			OutError = FText::Format(NSLOCTEXT("SandwichSteam", "SessionUpdateBelowCurrent", "Max Players cannot go below the {0} player(s) already in the session."), FText::AsNumber(CurrentPlayers));
			return false;
		}

		if (OutApplied.bUsesPresence != Current.bUsesPresence)
		{
			OutError = NSLOCTEXT("SandwichSteam", "SessionUpdatePresence", "Uses Presence cannot change after the session was created.");
			return false;
		}

		return true;
	}

	// ---- Party lobby (Phase 17) ----

	FSteamId ResolveLobbyTarget(ESteamLobbyTarget Target, const FSteamId& Party, const FSteamId& Session)
	{
		switch (Target)
		{
		case ESteamLobbyTarget::Party: return Party;
		case ESteamLobbyTarget::Session: return Session;
		default: return Party.IsValid() ? Party : Session;
		}
	}

	FString EncodePartyMatch(const FSteamPartyMatch& Match)
	{
		TArray<FString> Parts;
		if (Match.SessionLobby.IsValid())
		{
			Parts.Add(TEXT("lobby=") + Match.SessionLobby.ToString());
		}
		if (Match.ServerSteamId.IsValid())
		{
			Parts.Add(TEXT("server=") + Match.ServerSteamId.ToString());
		}
		const FString Address = Match.Address.TrimStartAndEnd();
		if (!Address.IsEmpty() && !Address.Contains(TEXT(";")) && !Address.Contains(TEXT("=")))
		{
			Parts.Add(TEXT("addr=") + Address);
		}
		return FString::Join(Parts, TEXT(";"));
	}

	bool DecodePartyMatch(const FString& Text, FSteamPartyMatch& OutMatch)
	{
		OutMatch = FSteamPartyMatch();

		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(";"));
		for (const FString& Part : Parts)
		{
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value) || Value.IsEmpty())
			{
				continue;
			}

			if (Key == TEXT("lobby"))
			{
				FSteamId::FromString(Value, OutMatch.SessionLobby);
			}
			else if (Key == TEXT("server"))
			{
				FSteamId::FromString(Value, OutMatch.ServerSteamId);
			}
			else if (Key == TEXT("addr"))
			{
				OutMatch.Address = Value;
			}
		}

		if (!OutMatch.IsValid())
		{
			OutMatch = FSteamPartyMatch();
			return false;
		}
		return true;
	}

	EPartyLobbyType ToPartyLobbyType(ESteamSessionVisibility Visibility)
	{
		switch (Visibility)
		{
		case ESteamSessionVisibility::FriendsOnly: return EPartyLobbyType::FriendsOnly;
		case ESteamSessionVisibility::Private: return EPartyLobbyType::Private;
		default: return EPartyLobbyType::Public;
		}
	}

	FPartyLobbyState MakeMatchLobbyState(const FPartyLobbyState& Before, ESteamPartyMatchVisibility Visibility, bool bClose)
	{
		FPartyLobbyState State = Before;
		switch (Visibility)
		{
		case ESteamPartyMatchVisibility::FriendsOnly: State.Type = EPartyLobbyType::FriendsOnly; break;
		case ESteamPartyMatchVisibility::Private: State.Type = EPartyLobbyType::Private; break;
		case ESteamPartyMatchVisibility::Invisible: State.Type = EPartyLobbyType::Invisible; break;
		default: break;
		}

		if (bClose)
		{
			State.bJoinable = false;
		}
		return State;
	}

	FString EncodeSavedLobbyState(const FPartyLobbyState& State)
	{
		return FString::Printf(TEXT("%d,%d"), static_cast<int32>(State.Type), State.bJoinable ? 1 : 0);
	}

	bool DecodeSavedLobbyState(const FString& Text, FPartyLobbyState& OutState)
	{
		FString TypeText;
		FString JoinableText;
		if (!Text.Split(TEXT(","), &TypeText, &JoinableText) || !TypeText.IsNumeric() || !JoinableText.IsNumeric())
		{
			return false;
		}

		const int32 Type = FCString::Atoi(*TypeText);
		const int32 Joinable = FCString::Atoi(*JoinableText);
		if (Type < 0 || Type > static_cast<int32>(EPartyLobbyType::Invisible) || (Joinable != 0 && Joinable != 1))
		{
			return false;
		}

		OutState.Type = static_cast<EPartyLobbyType>(Type);
		OutState.bJoinable = Joinable == 1;
		return true;
	}

	EPartyFollow DecideFollow(const FSteamPartyMatch& Match, const FSteamId& LocalId, const FSteamId& CurrentSessionLobby)
	{
		if (Match.SessionLobby.IsValid() && Match.SessionLobby == CurrentSessionLobby)
		{
			return EPartyFollow::None;
		}

		// The server Steam ID beats the session lobby: joining a lobby by its ID needs FindSessionById, a stub that
		// always fails in the Steam Online Subsystem. The session lobby is followed only when nothing else is known.
		if (Match.ServerSteamId.IsValid())
		{
			return Match.ServerSteamId == LocalId ? EPartyFollow::None : EPartyFollow::SteamId;
		}

		if (Match.SessionLobby.IsValid())
		{
			return EPartyFollow::Session;
		}

		return Match.Address.IsEmpty() ? EPartyFollow::None : EPartyFollow::Address;
	}

	bool ParseIPv4Address(const FString& Address, uint32& OutIp, uint16& OutPort)
	{
		OutIp = 0;
		OutPort = 0;

		FString Host;
		FString PortText;
		if (!Address.TrimStartAndEnd().Split(TEXT(":"), &Host, &PortText) || !PortText.IsNumeric())
		{
			return false;
		}

		const int32 Port = FCString::Atoi(*PortText);
		TArray<FString> Octets;
		Host.ParseIntoArray(Octets, TEXT("."), /*bCullEmpty*/ false);
		if (Port <= 0 || Port > 65535 || Octets.Num() != 4)
		{
			return false;
		}

		uint32 Ip = 0;
		for (const FString& Octet : Octets)
		{
			if (Octet.IsEmpty() || Octet.Len() > 3 || !Octet.IsNumeric())
			{
				return false;
			}
			const int32 Value = FCString::Atoi(*Octet);
			if (Value > 255)
			{
				return false;
			}
			Ip = (Ip << 8) | static_cast<uint32>(Value);
		}

		OutIp = Ip;
		OutPort = static_cast<uint16>(Port);
		return true;
	}

	FString FormatIPv4Address(uint32 Ip, uint16 Port)
	{
		return FString::Printf(TEXT("%u.%u.%u.%u:%u"), (Ip >> 24) & 0xFF, (Ip >> 16) & 0xFF, (Ip >> 8) & 0xFF, Ip & 0xFF, static_cast<uint32>(Port));
	}
}
