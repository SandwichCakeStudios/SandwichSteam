// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamSessions.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "GameplayTagsManager.h"
#include "SteamSessionsSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	/** The last search of Steam.Sessions.Find, so Steam.Sessions.Join can take an index. */
	TArray<FSteamSessionResult> GLastFound;

	void DumpSessions(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			Output.Log(*Sessions->BuildDebugString());
		}
	}

	void LogResult(FOutputDevice& Output, const TCHAR* Command, const FSteamResult& Result, const TCHAR* Started)
	{
		Output.Logf(TEXT("%s: %s"), Command, Result.IsSuccess() ? Started : *Result.Message.ToString());
	}

	/** Steam.Sessions.Create <ProfileTag>: hosts a session of a profile of the App Definition. */
	void CreateSession(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		const FGameplayTag Tag = Args.IsEmpty() ? FGameplayTag() : UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Create <ProfileTag>, for example Steam.Sessions.Create Steam.Session.Coop (the tag must be a session profile of the App Definition)."));
			return;
		}

		const FSteamResult Result = Sessions->CreateSession(Tag, FSteamSessionOpDelegate::CreateLambda([](const FSteamResult& Done)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.Create: %s"), Done.IsSuccess() ? TEXT("session created. Use Steam.Sessions.Dump for details.") : *Done.Message.ToString());
		}));
		LogResult(Output, TEXT("Steam.Sessions.Create"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	/** Parses "Public" / "Friends" / "Private" (case-insensitive). Defaults to Public on anything else. */
	ESteamSessionVisibility ParseVisibility(const FString& Text)
	{
		if (Text.StartsWith(TEXT("Friend"), ESearchCase::IgnoreCase)) { return ESteamSessionVisibility::FriendsOnly; }
		if (Text.StartsWith(TEXT("Private"), ESearchCase::IgnoreCase)) { return ESteamSessionVisibility::Private; }
		return ESteamSessionVisibility::Public;
	}

	/** The arguments from Start on as one text (values may hold spaces). Declared here too: used before the lobby extras section below. */
	FString JoinArgsFrom(const TArray<FString>& Args, int32 Start)
	{
		FString Text;
		for (int32 Index = Start; Index < Args.Num(); ++Index)
		{
			Text += Index > Start ? TEXT(" ") + Args[Index] : Args[Index];
		}
		return Text;
	}

	/** Steam.Sessions.CreateWith <ProfileTag|-> <Players> <Public|Friends|Private> [Name...]: create with player-chosen settings. */
	void CreateSessionWith(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		if (Args.Num() < 3)
		{
			Output.Log(TEXT("Usage: Steam.Sessions.CreateWith <ProfileTag|-> <Players> <Public|Friends|Private> [Name...]. Use - for no profile (full control)."));
			return;
		}

		const FGameplayTag Tag = Args[0] == TEXT("-") ? FGameplayTag() : UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		FSteamSessionSettings Settings;
		Settings.MaxPlayers = FCString::Atoi(*Args[1]);
		Settings.Visibility = ParseVisibility(Args[2]);
		Settings.DisplayName = JoinArgsFrom(Args, 3);

		const auto OnDone = FSteamSessionSettingsOpDelegate::CreateLambda([](const FSteamResult& Done, const FSteamSessionSettings& Applied)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.CreateWith: %s"), Done.IsSuccess()
				? *FString::Printf(TEXT("session created, applied name '%s', %d players, visibility %d"), *Applied.DisplayName, Applied.MaxPlayers, static_cast<int32>(Applied.Visibility))
				: *FString::Printf(TEXT("%s (%s)"), *Done.Message.ToString(), *Done.ErrorTag.ToString()));
		});

		const FSteamResult Result = Tag.IsValid() ? Sessions->CreateSession(Tag, Settings, OnDone) : Sessions->CreateSession(Settings, OnDone);
		LogResult(Output, TEXT("Steam.Sessions.CreateWith"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	/** Steam.Sessions.Update <Players> <Public|Friends|Private> [Name...]: changes the running session's settings. */
	void UpdateSession(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		if (Args.Num() < 2)
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Update <Players> <Public|Friends|Private> [Name...]"));
			return;
		}

		FSteamSessionSettings Settings;
		Settings.MaxPlayers = FCString::Atoi(*Args[0]);
		Settings.Visibility = ParseVisibility(Args[1]);
		Settings.DisplayName = JoinArgsFrom(Args, 2);

		const FSteamResult Result = Sessions->UpdateSession(Settings, FSteamSessionSettingsOpDelegate::CreateLambda([](const FSteamResult& Done, const FSteamSessionSettings& Applied)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.Update: %s"), Done.IsSuccess()
				? *FString::Printf(TEXT("updated, applied name '%s', %d players, visibility %d"), *Applied.DisplayName, Applied.MaxPlayers, static_cast<int32>(Applied.Visibility))
				: *FString::Printf(TEXT("%s (%s)"), *Done.Message.ToString(), *Done.ErrorTag.ToString()));
		}));
		LogResult(Output, TEXT("Steam.Sessions.Update"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	/** Steam.Sessions.Find [ProfileTag]: searches lobbies and lists them. */
	void FindSessions(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		FSteamSessionSearchOptions Options;
		if (!Args.IsEmpty())
		{
			Options.ProfileTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		}

		const FSteamResult Result = Sessions->FindSessions(Options, FSteamSessionFindDelegate::CreateLambda([](const FSteamResult& Done, const TArray<FSteamSessionResult>& Found)
		{
			GLastFound = Found;
			if (!Done.IsSuccess())
			{
				UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.Find: %s"), *Done.Message.ToString());
				return;
			}

			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.Find: %d session(s). Join one with Steam.Sessions.Join <index>."), Found.Num());
			for (int32 Index = 0; Index < Found.Num(); ++Index)
			{
				const FSteamSessionResult& Session = Found[Index];
				UE_LOG(LogSandwichSteam, Display, TEXT("  [%d] host %s (%s), lobby %s, %d/%d free, ping %d ms, %d setting(s)"), Index, *Session.OwnerName, *Session.OwnerId.ToString(),
					*Session.LobbyId.ToString(), Session.OpenSlots, Session.MaxPlayers, Session.PingMs, Session.Settings.Num());
			}
		}));
		LogResult(Output, TEXT("Steam.Sessions.Find"), Result, TEXT("search started, the list is logged when Steam answers"));
	}

	/** Steam.Sessions.Join <index>: joins a session of the last Steam.Sessions.Find. */
	void JoinSession(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		const int32 Index = Args.IsEmpty() ? INDEX_NONE : FCString::Atoi(*Args[0]);
		if (!GLastFound.IsValidIndex(Index))
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Join <index>. Run Steam.Sessions.Find first and use an index from its list."));
			return;
		}

		const FSteamResult Result = Sessions->JoinSession(GLastFound[Index], FSteamSessionJoinDelegate::CreateLambda([](const FSteamResult& Done, const FString& Connect)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.Join: %s"), Done.IsSuccess() ? *FString::Printf(TEXT("joined, connect string %s"), *Connect) : *Done.Message.ToString());
		}));
		LogResult(Output, TEXT("Steam.Sessions.Join"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	void DestroySession(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		const FSteamResult Result = Sessions->DestroySession(FSteamSessionOpDelegate::CreateLambda([](const FSteamResult& Done)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.Destroy: %s"), Done.IsSuccess() ? TEXT("done") : *Done.Message.ToString());
		}));
		LogResult(Output, TEXT("Steam.Sessions.Destroy"), Result, TEXT("request sent"));
	}

	ESteamLobbyTarget TakeTarget(TArray<FString>& Args);

	/** Steam.Sessions.Invite [party|session] <SteamId>: invites a friend to the party, else the current session. */
	void InviteFriend(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		FSteamId Friend;
		if (Args.IsEmpty() || !FSteamId::FromString(Args[0], Friend))
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Invite [party|session] <SteamID64>"));
			return;
		}

		LogResult(Output, TEXT("Steam.Sessions.Invite"), Sessions->SendInvite(Friend, Target), TEXT("invite sent"));
	}

	void ShowInviteOverlay(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			TArray<FString> Args = InArgs;
			LogResult(Output, TEXT("Steam.Sessions.InviteOverlay"), Sessions->ShowInviteOverlay(TakeTarget(Args)), TEXT("overlay opened"));
		}
	}

	void AcceptRequest(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			LogResult(Output, TEXT("Steam.Sessions.Accept"), Sessions->AcceptJoinRequest(), TEXT("joining"));
		}
	}

	void DeclineRequest(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			Sessions->DeclineJoinRequest();
			Output.Log(TEXT("Steam.Sessions.Decline: the waiting request was dropped."));
		}
	}

	/** Steam.Sessions.Travel <Map>: hosts the map with ?listen (ServerTravel). */
	void HostTravel(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Travel <MapName>"));
			return;
		}

		LogResult(Output, TEXT("Steam.Sessions.Travel"), Sessions->ServerTravel(Args[0], /*bListen*/ true), TEXT("travelling"));
	}

	// ---- Lobby extras ----

	/** The last search of Steam.Sessions.LobbyFind, so Steam.Sessions.LobbyJoin can take an index. */
	TArray<FSteamLobbyInfo> GLastLobbies;

	/** The arguments from Start on as one text (values may hold spaces). */
	FString JoinFrom(const TArray<FString>& Args, int32 Start)
	{
		FString Text;
		for (int32 Index = Start; Index < Args.Num(); ++Index)
		{
			Text += Index > Start ? TEXT(" ") + Args[Index] : Args[Index];
		}
		return Text;
	}

	/**
	 * Optional first argument "party" or "session" of the lobby commands: removes it from Args and returns the target.
	 * Without it the command works on the active lobby (the party when there is one, else the session lobby).
	 */
	ESteamLobbyTarget TakeTarget(TArray<FString>& Args)
	{
		if (!Args.IsEmpty())
		{
			if (Args[0].Equals(TEXT("party"), ESearchCase::IgnoreCase))
			{
				Args.RemoveAt(0);
				return ESteamLobbyTarget::Party;
			}
			if (Args[0].Equals(TEXT("session"), ESearchCase::IgnoreCase))
			{
				Args.RemoveAt(0);
				return ESteamLobbyTarget::Session;
			}
		}
		return ESteamLobbyTarget::Active;
	}

	/** Steam.Sessions.LobbyCreate <ProfileTag>: the party lobby (a lobby without a net session). */
	void CreateLobby(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		const FGameplayTag Tag = Args.IsEmpty() ? FGameplayTag() : UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.LobbyCreate <ProfileTag>, for example Steam.Sessions.LobbyCreate Steam.Session.Coop."));
			return;
		}

		const FSteamResult Result = Sessions->CreateLobby(Tag, FSteamLobbyOpDelegate::CreateLambda([](const FSteamResult& Done, FSteamId Lobby)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.LobbyCreate: %s"), Done.IsSuccess() ? *FString::Printf(TEXT("lobby %s created"), *Lobby.ToString()) : *Done.Message.ToString());
		}));
		LogResult(Output, TEXT("Steam.Sessions.LobbyCreate"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	/** Steam.Sessions.LobbyFind [ProfileTag]: searches lobbies (party lobbies). */
	void FindLobbies(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		FSteamSessionSearchOptions Options;
		if (!Args.IsEmpty())
		{
			Options.ProfileTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		}

		const FSteamResult Result = Sessions->FindLobbies(Options, FSteamLobbyFindDelegate::CreateLambda([](const FSteamResult& Done, const TArray<FSteamLobbyInfo>& Found)
		{
			GLastLobbies = Found;
			if (!Done.IsSuccess())
			{
				UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.LobbyFind: %s"), *Done.Message.ToString());
				return;
			}

			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.LobbyFind: %d lobby(ies). Join one with Steam.Sessions.LobbyJoin <index>."), Found.Num());
			for (int32 Index = 0; Index < Found.Num(); ++Index)
			{
				const FSteamLobbyInfo& Lobby = Found[Index];
				UE_LOG(LogSandwichSteam, Display, TEXT("  [%d] lobby %s, owner %s, %d/%d members, %d data key(s)"), Index, *Lobby.LobbyId.ToString(), *Lobby.OwnerId.ToString(), Lobby.MemberCount, Lobby.MaxMembers, Lobby.Data.Num());
			}
		}));
		LogResult(Output, TEXT("Steam.Sessions.LobbyFind"), Result, TEXT("search started, the list is logged when Steam answers"));
	}

	/** Steam.Sessions.LobbyJoin <index | SteamID64>. */
	void JoinLobby(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		FSteamId Lobby;
		if (!Args.IsEmpty())
		{
			// Small numbers are indexes of the last search; anything else is a lobby ID.
			const int32 Index = FCString::Atoi(*Args[0]);
			if (Args[0].Len() < 4 && GLastLobbies.IsValidIndex(Index))
			{
				Lobby = GLastLobbies[Index].LobbyId;
			}
			else
			{
				FSteamId::FromString(Args[0], Lobby);
			}
		}

		if (!Lobby.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.LobbyJoin <index of Steam.Sessions.LobbyFind | lobby SteamID64>"));
			return;
		}

		const FSteamResult Result = Sessions->JoinLobby(Lobby, FSteamLobbyOpDelegate::CreateLambda([](const FSteamResult& Done, FSteamId Entered)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.LobbyJoin: %s"), Done.IsSuccess() ? *FString::Printf(TEXT("entered lobby %s"), *Entered.ToString()) : *FString::Printf(TEXT("%s (%s)"), *Done.Message.ToString(), *Done.ErrorTag.ToString()));
		}));
		LogResult(Output, TEXT("Steam.Sessions.LobbyJoin"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	void LeaveLobby(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			LogResult(Output, TEXT("Steam.Sessions.LobbyLeave"), Sessions->LeaveLobby(), TEXT("left the party lobby"));
		}
	}

	/** Steam.Sessions.Chat [party|session] <text>: sends a chat message to the lobby. */
	void SendChat(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Chat [party|session] <text>"));
			return;
		}

		LogResult(Output, TEXT("Steam.Sessions.Chat"), Sessions->SendChatMessage(FString::Join(Args, TEXT(" ")), Target), TEXT("sent, it comes back through the chat event"));
	}

	/** Steam.Sessions.Ready [party|session] [0|1]: the local ready state (default 1). */
	void SetReady(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			TArray<FString> Args = InArgs;
			const ESteamLobbyTarget Target = TakeTarget(Args);
			const bool bReady = Args.IsEmpty() || FCString::Atoi(*Args[0]) != 0;
			LogResult(Output, TEXT("Steam.Sessions.Ready"), Sessions->SetReady(bReady, Target), bReady ? TEXT("ready") : TEXT("not ready"));
		}
	}

	/** Steam.Sessions.LobbyData [party|session] [key [value]]: no argument lists, a key reads, a key and a value (owner only) writes. */
	void LobbyData(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		if (Args.Num() >= 2)
		{
			LogResult(Output, TEXT("Steam.Sessions.LobbyData"), Sessions->SetLobbyData(Args[0], JoinFrom(Args, 1), Target), TEXT("set"));
			return;
		}

		if (Args.Num() == 1)
		{
			FString Value;
			Output.Logf(TEXT("Steam.Sessions.LobbyData: %s = %s"), *Args[0], Sessions->GetLobbyData(Args[0], Value, Target) ? *Value : TEXT("(not set)"));
			return;
		}

		const TMap<FString, FString> Data = Sessions->GetAllLobbyData(Target);
		Output.Logf(TEXT("Steam.Sessions.LobbyData: %d key(s)"), Data.Num());
		for (const TPair<FString, FString>& Pair : Data)
		{
			Output.Logf(TEXT("  %s = %s"), *Pair.Key, *Pair.Value.Left(120));
		}
	}

	/** Steam.Sessions.MemberData [party|session] <key> [value]: with a value sets the local member data, without reads it for every member. */
	void MemberData(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.MemberData [party|session] <key> [value]"));
			return;
		}

		if (Args.Num() >= 2)
		{
			LogResult(Output, TEXT("Steam.Sessions.MemberData"), Sessions->SetMemberData(Args[0], JoinFrom(Args, 1), Target), TEXT("set"));
			return;
		}

		for (const FSteamLobbyMember& Member : Sessions->GetLobbyMembers(Target))
		{
			Output.Logf(TEXT("  %s %s: %s = %s"), *Member.Id.ToString(), *Member.Name, *Args[0], *Sessions->GetMemberData(Member.Id, Args[0], Target));
		}
	}

	/** Steam.Sessions.Joinable [party|session] <0|1>: owner only, closes or opens the lobby for new members. */
	void SetJoinable(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Joinable [party|session] <0|1>"));
			return;
		}

		LogResult(Output, TEXT("Steam.Sessions.Joinable"), Sessions->SetLobbyJoinable(FCString::Atoi(*Args[0]) != 0, Target), TEXT("changed"));
	}

	/** Steam.Sessions.Kick [party|session] <SteamID64> [reason] and Steam.Sessions.Forgive [party|session] <SteamID64>. Owner only. */
	void KickMember(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		FSteamId Member;
		if (Args.IsEmpty() || !FSteamId::FromString(Args[0], Member))
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Kick [party|session] <SteamID64> [reason]"));
			return;
		}

		const FString Reason = Args.Num() > 1 ? JoinFrom(Args, 1) : FString();
		LogResult(Output, TEXT("Steam.Sessions.Kick"), Sessions->KickMember(Member, Reason, Target), TEXT("kick marker written"));
	}

	void ForgiveMember(const TArray<FString>& InArgs, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		TArray<FString> Args = InArgs;
		const ESteamLobbyTarget Target = TakeTarget(Args);
		FSteamId Member;
		if (Args.IsEmpty() || !FSteamId::FromString(Args[0], Member))
		{
			Output.Log(TEXT("Usage: Steam.Sessions.Forgive [party|session] <SteamID64>"));
			return;
		}

		LogResult(Output, TEXT("Steam.Sessions.Forgive"), Sessions->ForgiveMember(Member, Target), TEXT("kick marker removed"));
	}

	// ---- Party lobby (Phase 17) ----

	/** Steam.Sessions.PartyMatchBegin [session | steam <SteamID64> | addr <ip:port>]: owner only. No argument = the current session's lobby. */
	void PartyMatchBegin(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		FSteamPartyMatch Match;
		const FString Kind = Args.IsEmpty() ? FString(TEXT("session")) : Args[0];
		if (Kind.Equals(TEXT("session"), ESearchCase::IgnoreCase))
		{
			Match.SessionLobby = Sessions->GetSessionLobbyId();
		}
		else if (Kind.Equals(TEXT("steam"), ESearchCase::IgnoreCase) && Args.Num() >= 2)
		{
			FSteamId::FromString(Args[1], Match.ServerSteamId);
		}
		else if (Kind.Equals(TEXT("addr"), ESearchCase::IgnoreCase) && Args.Num() >= 2)
		{
			Match.Address = Args[1];
		}

		if (!Match.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.PartyMatchBegin [session | steam <SteamID64> | addr <ip:port>]. 'session' needs a current session with a lobby."));
			return;
		}

		LogResult(Output, TEXT("Steam.Sessions.PartyMatchBegin"), Sessions->BeginPartyMatch(Match), TEXT("party match begun, members get On Party Match Started"));
	}

	void PartyMatchEnd(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			LogResult(Output, TEXT("Steam.Sessions.PartyMatchEnd"), Sessions->EndPartyMatch(), TEXT("party match ended, lobby state restored"));
		}
	}

	void PartyFollow(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output))
		{
			LogResult(Output, TEXT("Steam.Sessions.PartyFollow"), Sessions->FollowPartyMatch(), TEXT("following the party match"));
		}
	}

	/** Steam.Sessions.PartyHost <ProfileTag>: owner only, Host Party Session (create a session and begin the party match with it). */
	void PartyHost(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		const FGameplayTag Tag = Args.IsEmpty() ? FGameplayTag() : UGameplayTagsManager::Get().RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		if (!Tag.IsValid())
		{
			Output.Log(TEXT("Usage: Steam.Sessions.PartyHost <ProfileTag>. Run it after Steam.Sessions.Travel <Map> so the members can connect at once."));
			return;
		}

		const FSteamResult Result = Sessions->HostPartySession(Tag, FSteamSessionOpDelegate::CreateLambda([](const FSteamResult& Done)
		{
			UE_LOG(LogSandwichSteam, Display, TEXT("Steam.Sessions.PartyHost: %s"), Done.IsSuccess() ? TEXT("session created and party match begun") : *Done.Message.ToString());
		}));
		LogResult(Output, TEXT("Steam.Sessions.PartyHost"), Result, TEXT("request sent, the result is logged when Steam answers"));
	}

	/** Steam.Sessions.ProbeLobby <SteamID64>: prints what Steam knows about any lobby and asks for fresh data (run it again to see it). */
	void ProbeLobby(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamSessionsSubsystem* Sessions = FSteamDebugCommandSet::FindFeature<USteamSessionsSubsystem>(World, Output);
		if (!Sessions)
		{
			return;
		}

		FSteamId Lobby;
		if (Args.IsEmpty() || !FSteamId::FromString(Args[0], Lobby))
		{
			Output.Log(TEXT("Usage: Steam.Sessions.ProbeLobby <lobby SteamID64>"));
			return;
		}

		Output.Log(*Sessions->DebugProbeLobby(Lobby));
	}

	const FName DebugSectionId(TEXT("Sessions"));

	FString ReportSessions(UWorld* World)
	{
		const USteamSessionsSubsystem* Sessions = SandwichSteam::Debug::FindFeatureSubsystem<USteamSessionsSubsystem>(World);
		return Sessions ? Sessions->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamSessionsModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamSessions", "DebugTitle", "Sessions");
	Section.Order = 90;
	Section.BuildReport = &ReportSessions;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Sessions.Dump"),
		TEXT("Prints the session profiles, the current session, pending requests and counters."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpSessions));

	Commands.Add(TEXT("Steam.Sessions.Create"),
		TEXT("Hosts a session of an App Definition profile: Steam.Sessions.Create <ProfileTag>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CreateSession));

	Commands.Add(TEXT("Steam.Sessions.CreateWith"),
		TEXT("Hosts a session with player-chosen settings: Steam.Sessions.CreateWith <ProfileTag|-> <Players> <Public|Friends|Private> [Name...]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CreateSessionWith));

	Commands.Add(TEXT("Steam.Sessions.Update"),
		TEXT("Changes the running session's settings: Steam.Sessions.Update <Players> <Public|Friends|Private> [Name...]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&UpdateSession));

	Commands.Add(TEXT("Steam.Sessions.Find"),
		TEXT("Searches lobbies and lists them: Steam.Sessions.Find [ProfileTag]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&FindSessions));

	Commands.Add(TEXT("Steam.Sessions.Join"),
		TEXT("Joins a session of the last Steam.Sessions.Find: Steam.Sessions.Join <index>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&JoinSession));

	Commands.Add(TEXT("Steam.Sessions.Destroy"),
		TEXT("Leaves the current session, or closes it when hosting."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DestroySession));

	Commands.Add(TEXT("Steam.Sessions.Invite"),
		TEXT("Invites a friend to the party, else the current session: Steam.Sessions.Invite [party|session] <SteamID64>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&InviteFriend));

	Commands.Add(TEXT("Steam.Sessions.InviteOverlay"),
		TEXT("Opens the Steam overlay invite dialog for the party, else the current session: Steam.Sessions.InviteOverlay [party|session]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ShowInviteOverlay));

	Commands.Add(TEXT("Steam.Sessions.Accept"),
		TEXT("Joins the join request that waits for the game (policy Ask Game)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&AcceptRequest));

	Commands.Add(TEXT("Steam.Sessions.Decline"),
		TEXT("Drops the join request that waits for the game."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DeclineRequest));

	Commands.Add(TEXT("Steam.Sessions.Travel"),
		TEXT("Loads a map as the host (with ?listen): Steam.Sessions.Travel <MapName>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HostTravel));

	Commands.Add(TEXT("Steam.Sessions.LobbyCreate"),
		TEXT("Creates the party lobby (a lobby without a net session): Steam.Sessions.LobbyCreate <ProfileTag>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CreateLobby));

	Commands.Add(TEXT("Steam.Sessions.LobbyFind"),
		TEXT("Searches lobbies (party lobbies) and lists them: Steam.Sessions.LobbyFind [ProfileTag]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&FindLobbies));

	Commands.Add(TEXT("Steam.Sessions.LobbyJoin"),
		TEXT("Joins a lobby as the party, by index of the last LobbyFind or by SteamID64: Steam.Sessions.LobbyJoin <index|id>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&JoinLobby));

	Commands.Add(TEXT("Steam.Sessions.LobbyLeave"),
		TEXT("Leaves the party lobby, also during a session (a session lobby is left with Steam.Sessions.Destroy)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&LeaveLobby));

	Commands.Add(TEXT("Steam.Sessions.Chat"),
		TEXT("Sends a chat message: Steam.Sessions.Chat [party|session] <text>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SendChat));

	Commands.Add(TEXT("Steam.Sessions.Ready"),
		TEXT("Sets the local ready state of the lobby: Steam.Sessions.Ready [party|session] [0|1]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetReady));

	Commands.Add(TEXT("Steam.Sessions.LobbyData"),
		TEXT("Lists (no argument), reads (key) or, as the owner, writes (key value) lobby data: Steam.Sessions.LobbyData [party|session] [key [value]]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&LobbyData));

	Commands.Add(TEXT("Steam.Sessions.MemberData"),
		TEXT("Reads (key) the member data of every member or sets (key value) the local one: Steam.Sessions.MemberData [party|session] <key> [value]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&MemberData));

	Commands.Add(TEXT("Steam.Sessions.Joinable"),
		TEXT("Owner only: closes (0) or opens (1) the lobby for new members: Steam.Sessions.Joinable [party|session] <0|1>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetJoinable));

	Commands.Add(TEXT("Steam.Sessions.Kick"),
		TEXT("Owner only: kicks a player by the lobby data convention: Steam.Sessions.Kick [party|session] <SteamID64> [reason]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&KickMember));

	Commands.Add(TEXT("Steam.Sessions.Forgive"),
		TEXT("Owner only: removes the kick marker of a player: Steam.Sessions.Forgive [party|session] <SteamID64>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ForgiveMember));

	Commands.Add(TEXT("Steam.Sessions.PartyMatchBegin"),
		TEXT("Party owner only: begins a party match: Steam.Sessions.PartyMatchBegin [session | steam <SteamID64> | addr <ip:port>]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PartyMatchBegin));

	Commands.Add(TEXT("Steam.Sessions.PartyMatchEnd"),
		TEXT("Party owner only: ends the party match and restores the party's lobby type and joinable flag."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PartyMatchEnd));

	Commands.Add(TEXT("Steam.Sessions.PartyFollow"),
		TEXT("Joins the running party match (Session Lobby, Server Steam Id or Address)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PartyFollow));

	Commands.Add(TEXT("Steam.Sessions.PartyHost"),
		TEXT("Party owner only: creates a session of a profile and begins the party match with it: Steam.Sessions.PartyHost <ProfileTag>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&PartyHost));

	Commands.Add(TEXT("Steam.Sessions.ProbeLobby"),
		TEXT("Prints the lobby data Steam has for any lobby and asks for fresh data: Steam.Sessions.ProbeLobby <lobby SteamID64>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ProbeLobby));
#endif
}

void FSandwichSteamSessionsModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
	GLastFound.Reset();
	GLastLobbies.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamSessionsModule, SandwichSteamSessions)
