// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamLobbyBackend.h"
#include "Core/SteamCallResult.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamSDK.h"
#include "Data/SteamAppDefinition.h"
#include "GameplayTagsManager.h"
#include "SteamSessionRules.h"
#include "SteamSessionsSubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

/**
 * Steam callbacks and pending call results of the lobby backend. Steam calls the callbacks on its callback thread: copy the
 * payload (the chat text is read here, it belongs to the callback), dispatch, return.
 */
class FSteamLobbyRaw
{
public:
	FSteamLobbyRaw(USteamSessionsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
		: Owner(InOwner)
		, Dispatcher(InDispatcher)
	{
		DataUpdateCallback.Register(this, &FSteamLobbyRaw::OnLobbyDataUpdate);
		ChatUpdateCallback.Register(this, &FSteamLobbyRaw::OnLobbyChatUpdate);
		ChatMessageCallback.Register(this, &FSteamLobbyRaw::OnLobbyChatMessage);
	}

	FSteamLobbyRaw(const FSteamLobbyRaw&) = delete;
	FSteamLobbyRaw& operator=(const FSteamLobbyRaw&) = delete;

	TSteamCallResult<LobbyCreated_t>::FPendingList PendingCreate;
	TSteamCallResult<LobbyEnter_t>::FPendingList PendingEnter;
	TSteamCallResult<LobbyMatchList_t>::FPendingList PendingList;

private:
	STEAM_CALLBACK_MANUAL(FSteamLobbyRaw, OnLobbyDataUpdate, LobbyDataUpdate_t, DataUpdateCallback);
	STEAM_CALLBACK_MANUAL(FSteamLobbyRaw, OnLobbyChatUpdate, LobbyChatUpdate_t, ChatUpdateCallback);
	STEAM_CALLBACK_MANUAL(FSteamLobbyRaw, OnLobbyChatMessage, LobbyChatMsg_t, ChatMessageCallback);

	TWeakObjectPtr<USteamSessionsSubsystem> Owner;
	TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe> Dispatcher;
};

void FSteamLobbyRaw::OnLobbyDataUpdate(LobbyDataUpdate_t* Payload)
{
	const FSteamId Lobby(static_cast<int64>(Payload->m_ulSteamIDLobby));
	const FSteamId Member(static_cast<int64>(Payload->m_ulSteamIDMember));
	const bool bSuccess = Payload->m_bSuccess != 0;
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [Lobby, Member, bSuccess](USteamSessionsSubsystem& Sessions)
	{
		Sessions.HandleLobbyDataUpdate(Lobby, Member, bSuccess);
	});
}

void FSteamLobbyRaw::OnLobbyChatUpdate(LobbyChatUpdate_t* Payload)
{
	const FSteamId Lobby(static_cast<int64>(Payload->m_ulSteamIDLobby));
	const FSteamId Changed(static_cast<int64>(Payload->m_ulSteamIDUserChanged));
	const uint32 Flags = Payload->m_rgfChatMemberStateChange;
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [Lobby, Changed, Flags](USteamSessionsSubsystem& Sessions)
	{
		Sessions.HandleLobbyChatUpdate(Lobby, Changed, Flags);
	});
}

void FSteamLobbyRaw::OnLobbyChatMessage(LobbyChatMsg_t* Payload)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking)
	{
		return;
	}

	// The entry is only valid for this callback. One byte stays free for the terminator.
	char Buffer[SandwichSteam::Sessions::MaxChatBytes + 1] = {};
	CSteamID Sender;
	EChatEntryType EntryType = k_EChatEntryTypeInvalid;
	const int32 Length = Matchmaking->GetLobbyChatEntry(CSteamID(Payload->m_ulSteamIDLobby), Payload->m_iChatID, &Sender, Buffer, SandwichSteam::Sessions::MaxChatBytes, &EntryType);
	if (Length <= 0 || EntryType != k_EChatEntryTypeChatMsg)
	{
		return;
	}

	Buffer[FMath::Min(Length, SandwichSteam::Sessions::MaxChatBytes)] = '\0';
	const FSteamId Lobby(static_cast<int64>(Payload->m_ulSteamIDLobby));
	const FSteamId SenderId(static_cast<int64>(Sender.ConvertToUint64()));
	const FString Text = UTF8_TO_TCHAR(Buffer);
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [Lobby, SenderId, Text](USteamSessionsSubsystem& Sessions)
	{
		Sessions.HandleLobbyChatMessage(Lobby, SenderId, Text);
	});
}

namespace
{
	CSteamID ToSteam(FSteamId Id)
	{
		return CSteamID(static_cast<uint64>(Id.Value));
	}

	FSteamId FromSteam(const CSteamID& Id)
	{
		return FSteamId(static_cast<int64>(Id.ConvertToUint64()));
	}

	ELobbyDistanceFilter ToSteamDistance(ESteamSessionDistance Distance)
	{
		switch (Distance)
		{
		case ESteamSessionDistance::Close: return k_ELobbyDistanceFilterClose;
		case ESteamSessionDistance::Far: return k_ELobbyDistanceFilterFar;
		case ESteamSessionDistance::Worldwide: return k_ELobbyDistanceFilterWorldwide;
		default: return k_ELobbyDistanceFilterDefault;
		}
	}

	ELobbyType ToSteamLobbyType(ESteamSessionVisibility Visibility)
	{
		switch (Visibility)
		{
		case ESteamSessionVisibility::FriendsOnly: return k_ELobbyTypeFriendsOnly;
		case ESteamSessionVisibility::Private: return k_ELobbyTypePrivate;
		default: return k_ELobbyTypePublic;
		}
	}

	FSteamResult MakeSteamFailure(EResult Result)
	{
		const int32 Native = static_cast<int32>(Result);
		if (Result == k_EResultTimeout)
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_Timeout, NSLOCTEXT("SandwichSteam", "LobbyTimeout", "Steam did not answer in time."), Native);
		}
		if (Result == k_EResultNoConnection)
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_Offline, NSLOCTEXT("SandwichSteam", "LobbyOffline", "Steam is not connected."), Native);
		}
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyFailed", "Steam could not complete the lobby request."), Native);
	}

	FSteamResult MakeEnterResult(int32 Response)
	{
		using SandwichSteam::Sessions::ELobbyEnterOutcome;
		switch (SandwichSteam::Sessions::ClassifyLobbyEnter(Response))
		{
		case ELobbyEnterOutcome::Success:
			return FSteamResult::Success();
		case ELobbyEnterOutcome::NotFound:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_NotFound, NSLOCTEXT("SandwichSteam", "LobbyEnterNotFound", "The lobby does not exist any more."), Response);
		case ELobbyEnterOutcome::Full:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_Full, NSLOCTEXT("SandwichSteam", "LobbyEnterFull", "The lobby is full."), Response);
		case ELobbyEnterOutcome::Denied:
			return FSteamResult::Failure(SteamGameplayTags::Error_Lobby_Denied, NSLOCTEXT("SandwichSteam", "LobbyEnterDenied", "Steam did not let the player into the lobby (closed, banned, blocked or a limited account)."), Response);
		default:
			return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "LobbyEnterFailed", "Steam could not enter the lobby."), Response);
		}
	}
}

FSteamLobbyBackend::FSteamLobbyBackend(USteamSessionsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	// Dedicated servers have no lobbies (no Steam client).
	if (!IsRunningDedicatedServer())
	{
		Raw = MakeUnique<FSteamLobbyRaw>(InOwner, InDispatcher);
	}
}

FSteamLobbyBackend::~FSteamLobbyBackend() = default;

bool FSteamLobbyBackend::IsUsable() const
{
	return Raw.IsValid() && SteamMatchmaking() && SteamUser();
}

FSteamId FSteamLobbyBackend::GetLocalId() const
{
	return SteamUser() ? FromSteam(SteamUser()->GetSteamID()) : FSteamId();
}

bool FSteamLobbyBackend::CreateLobby(const FGameplayTag& ProfileTag, const FSteamSessionSettings& Settings, FEnterDelegate OnDone)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !Raw.IsValid())
	{
		return false;
	}

	const int32 Slots = FMath::Clamp(Settings.MaxPlayers, 1, 250);
	const SteamAPICall_t Call = Matchmaking->CreateLobby(ToSteamLobbyType(Settings.Visibility), Slots);

	TMap<FString, FString> InitialData;
	if (ProfileTag.IsValid())
	{
		InitialData.Add(SandwichSteam::Sessions::ProfileKey(), ProfileTag.ToString());
	}
	InitialData.Add(SandwichSteam::Sessions::NameKey(), Settings.DisplayName);
	for (const TPair<FName, FString>& Pair : Settings.Settings)
	{
		if (!Pair.Key.IsNone())
		{
			InitialData.Add(Pair.Key.ToString(), Pair.Value);
		}
	}

	const TWeakPtr<FSteamLobbyBackend> WeakThis = AsShared();
	return TSteamCallResult<LobbyCreated_t>::Start(Raw->PendingCreate, Dispatcher, Call,
		[WeakThis, InitialData = MoveTemp(InitialData), OnDone = MoveTemp(OnDone)](const LobbyCreated_t& Result, bool bIOFailure)
		{
			const TSharedPtr<FSteamLobbyBackend> Backend = WeakThis.Pin();
			if (!Backend.IsValid())
			{
				return;
			}

			if (bIOFailure || Result.m_eResult != k_EResultOK)
			{
				OnDone(MakeSteamFailure(bIOFailure ? k_EResultNoConnection : Result.m_eResult), FSteamId());
				return;
			}

			const FSteamId Lobby(static_cast<int64>(Result.m_ulSteamIDLobby));
			for (const TPair<FString, FString>& Pair : InitialData)
			{
				Backend->SetLobbyData(Lobby, Pair.Key, Pair.Value);
			}
			OnDone(FSteamResult::Success(), Lobby);
		}) != nullptr;
}

bool FSteamLobbyBackend::JoinLobby(FSteamId LobbyId, FEnterDelegate OnDone)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !Raw.IsValid() || !LobbyId.IsValid())
	{
		return false;
	}

	return TSteamCallResult<LobbyEnter_t>::Start(Raw->PendingEnter, Dispatcher, Matchmaking->JoinLobby(ToSteam(LobbyId)),
		[OnDone = MoveTemp(OnDone)](const LobbyEnter_t& Result, bool bIOFailure)
		{
			if (bIOFailure)
			{
				OnDone(MakeSteamFailure(k_EResultNoConnection), FSteamId());
				return;
			}

			const FSteamResult Outcome = MakeEnterResult(static_cast<int32>(Result.m_EChatRoomEnterResponse));
			OnDone(Outcome, Outcome.IsSuccess() ? FSteamId(static_cast<int64>(Result.m_ulSteamIDLobby)) : FSteamId());
		}) != nullptr;
}

bool FSteamLobbyBackend::RequestLobbyList(const FSteamSessionSearchOptions& Options, FListDelegate OnDone)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !Raw.IsValid())
	{
		return false;
	}

	// The filters apply to the next list request only.
	Matchmaking->AddRequestLobbyListDistanceFilter(ToSteamDistance(Options.Distance));
	Matchmaking->AddRequestLobbyListResultCountFilter(FMath::Clamp(Options.MaxResults, 1, 50));
	Matchmaking->AddRequestLobbyListFilterSlotsAvailable(1);

	if (Options.ProfileTag.IsValid())
	{
		Matchmaking->AddRequestLobbyListStringFilter(TCHAR_TO_UTF8(SandwichSteam::Sessions::ProfileKey()), TCHAR_TO_UTF8(*Options.ProfileTag.ToString()), k_ELobbyComparisonEqual);
	}

	for (const TPair<FName, FString>& Filter : Options.Filters)
	{
		if (!Filter.Key.IsNone())
		{
			Matchmaking->AddRequestLobbyListStringFilter(TCHAR_TO_UTF8(*Filter.Key.ToString()), TCHAR_TO_UTF8(*Filter.Value), k_ELobbyComparisonEqual);
		}
	}

	const TWeakPtr<FSteamLobbyBackend> WeakThis = AsShared();
	return TSteamCallResult<LobbyMatchList_t>::Start(Raw->PendingList, Dispatcher, Matchmaking->RequestLobbyList(),
		[WeakThis, OnDone = MoveTemp(OnDone)](const LobbyMatchList_t& Result, bool bIOFailure)
		{
			const TSharedPtr<FSteamLobbyBackend> Backend = WeakThis.Pin();
			ISteamMatchmaking* Matchmaking = SteamMatchmaking();
			if (!Backend.IsValid() || !Matchmaking)
			{
				return;
			}

			if (bIOFailure)
			{
				OnDone(MakeSteamFailure(k_EResultNoConnection), TArray<FSteamLobbyInfo>());
				return;
			}

			TArray<FSteamLobbyInfo> Lobbies;
			for (uint32 Index = 0; Index < Result.m_nLobbiesMatching; ++Index)
			{
				Lobbies.Add(Backend->DescribeLobby(FromSteam(Matchmaking->GetLobbyByIndex(static_cast<int32>(Index)))));
			}
			OnDone(FSteamResult::Success(), MoveTemp(Lobbies));
		}) != nullptr;
}

void FSteamLobbyBackend::LeaveLobby(FSteamId LobbyId)
{
	if (ISteamMatchmaking* Matchmaking = SteamMatchmaking())
	{
		if (LobbyId.IsValid())
		{
			Matchmaking->LeaveLobby(ToSteam(LobbyId));
		}
	}
}

FSteamId FSteamLobbyBackend::GetOwner(FSteamId LobbyId) const
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return (Matchmaking && LobbyId.IsValid()) ? FromSteam(Matchmaking->GetLobbyOwner(ToSteam(LobbyId))) : FSteamId();
}

int32 FSteamLobbyBackend::GetMemberLimit(FSteamId LobbyId) const
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return (Matchmaking && LobbyId.IsValid()) ? Matchmaking->GetLobbyMemberLimit(ToSteam(LobbyId)) : 0;
}

void FSteamLobbyBackend::GetMemberIds(FSteamId LobbyId, TArray<FSteamId>& OutIds) const
{
	OutIds.Reset();
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !LobbyId.IsValid())
	{
		return;
	}

	const int32 Count = Matchmaking->GetNumLobbyMembers(ToSteam(LobbyId));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutIds.Add(FromSteam(Matchmaking->GetLobbyMemberByIndex(ToSteam(LobbyId), Index)));
	}
}

FString FSteamLobbyBackend::GetMemberName(FSteamId Member) const
{
	ISteamFriends* Friends = SteamFriends();
	return (Friends && Member.IsValid()) ? FString(UTF8_TO_TCHAR(Friends->GetFriendPersonaName(ToSteam(Member)))) : FString();
}

bool FSteamLobbyBackend::GetLobbyData(FSteamId LobbyId, const FString& Key, FString& OutValue) const
{
	OutValue.Reset();
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !LobbyId.IsValid())
	{
		return false;
	}

	const char* Value = Matchmaking->GetLobbyData(ToSteam(LobbyId), TCHAR_TO_UTF8(*Key));
	if (!Value || Value[0] == '\0')
	{
		return false;
	}

	OutValue = UTF8_TO_TCHAR(Value);
	return true;
}

void FSteamLobbyBackend::GetAllLobbyData(FSteamId LobbyId, TMap<FString, FString>& OutData) const
{
	OutData.Reset();
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !LobbyId.IsValid())
	{
		return;
	}

	const int32 Count = Matchmaking->GetLobbyDataCount(ToSteam(LobbyId));
	char Key[SandwichSteam::Sessions::MaxLobbyKeyBytes + 1];
	TArray<char> Value;
	Value.SetNumUninitialized(SandwichSteam::Sessions::MaxLobbyValueBytes + 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Key[0] = '\0';
		Value[0] = '\0';
		if (Matchmaking->GetLobbyDataByIndex(ToSteam(LobbyId), Index, Key, sizeof(Key), Value.GetData(), Value.Num()))
		{
			OutData.Add(UTF8_TO_TCHAR(Key), UTF8_TO_TCHAR(Value.GetData()));
		}
	}
}

bool FSteamLobbyBackend::SetLobbyData(FSteamId LobbyId, const FString& Key, const FString& Value)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return Matchmaking && LobbyId.IsValid() && Matchmaking->SetLobbyData(ToSteam(LobbyId), TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
}

bool FSteamLobbyBackend::DeleteLobbyData(FSteamId LobbyId, const FString& Key)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return Matchmaking && LobbyId.IsValid() && Matchmaking->DeleteLobbyData(ToSteam(LobbyId), TCHAR_TO_UTF8(*Key));
}

void FSteamLobbyBackend::SetMemberData(FSteamId LobbyId, const FString& Key, const FString& Value)
{
	if (ISteamMatchmaking* Matchmaking = SteamMatchmaking())
	{
		if (LobbyId.IsValid())
		{
			Matchmaking->SetLobbyMemberData(ToSteam(LobbyId), TCHAR_TO_UTF8(*Key), TCHAR_TO_UTF8(*Value));
		}
	}
}

FString FSteamLobbyBackend::GetMemberData(FSteamId LobbyId, FSteamId Member, const FString& Key) const
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !LobbyId.IsValid() || !Member.IsValid())
	{
		return FString();
	}

	const char* Value = Matchmaking->GetLobbyMemberData(ToSteam(LobbyId), ToSteam(Member), TCHAR_TO_UTF8(*Key));
	return Value ? FString(UTF8_TO_TCHAR(Value)) : FString();
}

bool FSteamLobbyBackend::SendChat(FSteamId LobbyId, const FString& Text)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !LobbyId.IsValid())
	{
		return false;
	}

	const FTCHARToUTF8 Utf8(*Text);
	return Matchmaking->SendLobbyChatMsg(ToSteam(LobbyId), Utf8.Get(), Utf8.Length());
}

bool FSteamLobbyBackend::SetJoinable(FSteamId LobbyId, bool bJoinable)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return Matchmaking && LobbyId.IsValid() && Matchmaking->SetLobbyJoinable(ToSteam(LobbyId), bJoinable);
}

bool FSteamLobbyBackend::SetLobbyType(FSteamId LobbyId, ESteamSessionVisibility Visibility)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return Matchmaking && LobbyId.IsValid() && Matchmaking->SetLobbyType(ToSteam(LobbyId), ToSteamLobbyType(Visibility));
}

bool FSteamLobbyBackend::SetLobbyMemberLimit(FSteamId LobbyId, int32 MaxPlayers)
{
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	return Matchmaking && LobbyId.IsValid() && Matchmaking->SetLobbyMemberLimit(ToSteam(LobbyId), FMath::Clamp(MaxPlayers, 1, 250));
}

FSteamLobbyInfo FSteamLobbyBackend::DescribeLobby(FSteamId LobbyId) const
{
	FSteamLobbyInfo Info;
	ISteamMatchmaking* Matchmaking = SteamMatchmaking();
	if (!Matchmaking || !LobbyId.IsValid())
	{
		return Info;
	}

	Info.LobbyId = LobbyId;
	Info.OwnerId = GetOwner(LobbyId);
	Info.MemberCount = Matchmaking->GetNumLobbyMembers(ToSteam(LobbyId));
	Info.MaxMembers = Matchmaking->GetLobbyMemberLimit(ToSteam(LobbyId));
	GetAllLobbyData(LobbyId, Info.Data);

	if (const FString* Name = Info.Data.Find(SandwichSteam::Sessions::NameKey()))
	{
		Info.DisplayName = *Name;
	}
	if (const FString* Profile = Info.Data.Find(SandwichSteam::Sessions::ProfileKey()))
	{
		Info.ProfileTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(**Profile), /*ErrorIfNotFound*/ false);
	}
	return Info;
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

// Without the Steamworks SDK there are no lobbies: the backend is never usable and every call refuses.
class FSteamLobbyRaw
{
};

FSteamLobbyBackend::FSteamLobbyBackend(USteamSessionsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

FSteamLobbyBackend::~FSteamLobbyBackend() = default;

bool FSteamLobbyBackend::IsUsable() const { return false; }
FSteamId FSteamLobbyBackend::GetLocalId() const { return FSteamId(); }
bool FSteamLobbyBackend::CreateLobby(const FGameplayTag&, const FSteamSessionSettings&, FEnterDelegate) { return false; }
bool FSteamLobbyBackend::JoinLobby(FSteamId, FEnterDelegate) { return false; }
bool FSteamLobbyBackend::RequestLobbyList(const FSteamSessionSearchOptions&, FListDelegate) { return false; }
void FSteamLobbyBackend::LeaveLobby(FSteamId) {}
FSteamId FSteamLobbyBackend::GetOwner(FSteamId) const { return FSteamId(); }
int32 FSteamLobbyBackend::GetMemberLimit(FSteamId) const { return 0; }
void FSteamLobbyBackend::GetMemberIds(FSteamId, TArray<FSteamId>& OutIds) const { OutIds.Reset(); }
FString FSteamLobbyBackend::GetMemberName(FSteamId) const { return FString(); }
bool FSteamLobbyBackend::GetLobbyData(FSteamId, const FString&, FString& OutValue) const { OutValue.Reset(); return false; }
void FSteamLobbyBackend::GetAllLobbyData(FSteamId, TMap<FString, FString>& OutData) const { OutData.Reset(); }
bool FSteamLobbyBackend::SetLobbyData(FSteamId, const FString&, const FString&) { return false; }
bool FSteamLobbyBackend::DeleteLobbyData(FSteamId, const FString&) { return false; }
void FSteamLobbyBackend::SetMemberData(FSteamId, const FString&, const FString&) {}
FString FSteamLobbyBackend::GetMemberData(FSteamId, FSteamId, const FString&) const { return FString(); }
bool FSteamLobbyBackend::SendChat(FSteamId, const FString&) { return false; }
bool FSteamLobbyBackend::SetJoinable(FSteamId, bool) { return false; }
bool FSteamLobbyBackend::SetLobbyType(FSteamId, ESteamSessionVisibility) { return false; }
bool FSteamLobbyBackend::SetLobbyMemberLimit(FSteamId, int32) { return false; }
FSteamLobbyInfo FSteamLobbyBackend::DescribeLobby(FSteamId) const { return FSteamLobbyInfo(); }

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
