// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamFriendsBackend.h"
#include "SteamFriendsSubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

namespace
{
	FSteamId ToSteamId(const CSteamID& Id)
	{
		return FSteamId(static_cast<int64>(Id.ConvertToUint64()));
	}

	/** Steam's persona flags reduced to the ones the plugin reports. Safe on any thread. */
	uint32 ToChangeFlags(int32 SteamFlags)
	{
		uint32 Flags = 0;
		if (SteamFlags & k_EPersonaChangeName)
		{
			Flags |= static_cast<uint32>(ESteamFriendChange::Name);
		}
		if (SteamFlags & (k_EPersonaChangeStatus | k_EPersonaChangeComeOnline | k_EPersonaChangeGoneOffline))
		{
			Flags |= static_cast<uint32>(ESteamFriendChange::State);
		}
		if (SteamFlags & (k_EPersonaChangeGamePlayed | k_EPersonaChangeGameServer))
		{
			Flags |= static_cast<uint32>(ESteamFriendChange::Game);
		}
		if (SteamFlags & k_EPersonaChangeAvatar)
		{
			Flags |= static_cast<uint32>(ESteamFriendChange::Avatar);
		}
		if (SteamFlags & k_EPersonaChangeRelationshipChanged)
		{
			Flags |= static_cast<uint32>(ESteamFriendChange::Relationship);
		}
		return Flags;
	}
}

FSteamFriendsBackend::FSteamFriendsBackend(USteamFriendsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	PersonaStateChangeCallback.Register(this, &FSteamFriendsBackend::OnPersonaStateChange);
}

void FSteamFriendsBackend::CollectIds(ESteamFriendSource Source, TArray<FSteamId>& OutIds) const
{
	ISteamFriends* Friends = SteamFriends();
	if (!Friends)
	{
		return;
	}

	if (Source == ESteamFriendSource::RecentPlayers)
	{
		const int32 Count = Friends->GetCoplayFriendCount();
		OutIds.Reserve(OutIds.Num() + Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutIds.Add(ToSteamId(Friends->GetCoplayFriend(Index)));
		}
		return;
	}

	const int32 Flags = Source == ESteamFriendSource::Blocked ? k_EFriendFlagBlocked : k_EFriendFlagImmediate;
	const int32 Count = Friends->GetFriendCount(Flags);
	OutIds.Reserve(OutIds.Num() + Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutIds.Add(ToSteamId(Friends->GetFriendByIndex(Index, Flags)));
	}
}

bool FSteamFriendsBackend::ReadInfo(FSteamId Id, bool bRequestIfMissing, FSteamFriendInfo& Out) const
{
	Out = FSteamFriendInfo();
	Out.SteamId = Id;

	ISteamFriends* Friends = SteamFriends();
	ISteamUtils* Utils = SteamUtils();
	if (!Friends || !Id.IsValid())
	{
		Out.bInfoLoaded = false;
		return false;
	}

	const CSteamID SteamId(static_cast<uint64>(Id.Value));

	// True while Steam fetches the name. Never the case for friends and the local user.
	const bool bFetching = bRequestIfMissing && Friends->RequestUserInformation(SteamId, /*bRequireNameOnly*/ true);
	Out.bInfoLoaded = !bFetching;

	Out.Name = UTF8_TO_TCHAR(Friends->GetFriendPersonaName(SteamId));

	// Steam's EPersonaState values are the enum values of ESteamPersonaState.
	Out.PersonaState = static_cast<ESteamPersonaState>(FMath::Clamp(static_cast<int32>(Friends->GetFriendPersonaState(SteamId)), 0, 7));

	FriendGameInfo_t Game;
	if (Friends->GetFriendGamePlayed(SteamId, &Game) && Game.m_gameID.IsValid())
	{
		Out.bInGame = true;
		Out.GameAppId = static_cast<int32>(Game.m_gameID.AppID());
		Out.bInThisGame = Utils && Game.m_gameID.AppID() == Utils->GetAppID();
		if (Game.m_steamIDLobby.IsValid())
		{
			Out.LobbyId = ToSteamId(Game.m_steamIDLobby);
		}
	}

	return Out.bInfoLoaded;
}

int32 FSteamFriendsBackend::GetFriendCount() const
{
	ISteamFriends* Friends = SteamFriends();
	return Friends ? Friends->GetFriendCount(k_EFriendFlagImmediate) : 0;
}

bool FSteamFriendsBackend::IsFriend(FSteamId Id) const
{
	ISteamFriends* Friends = SteamFriends();
	return Friends && Id.IsValid() && Friends->GetFriendRelationship(CSteamID(static_cast<uint64>(Id.Value))) == k_EFriendRelationshipFriend;
}

void FSteamFriendsBackend::ReadGroups(TArray<FSteamFriendGroup>& OutGroups) const
{
#if SANDWICHSTEAM_WITH_FRIEND_GROUPS
	ISteamFriends* Friends = SteamFriends();
	if (!Friends)
	{
		return;
	}

	const int32 GroupCount = Friends->GetFriendsGroupCount();
	for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		const FriendsGroupID_t GroupId = Friends->GetFriendsGroupIDByIndex(GroupIndex);

		FSteamFriendGroup& Group = OutGroups.AddDefaulted_GetRef();
		Group.GroupId = static_cast<int32>(GroupId);
		Group.Name = UTF8_TO_TCHAR(Friends->GetFriendsGroupName(GroupId));

		const int32 MemberCount = Friends->GetFriendsGroupMembersCount(GroupId);
		if (MemberCount > 0)
		{
			TArray<CSteamID> Members;
			Members.SetNum(MemberCount);
			Friends->GetFriendsGroupMembersList(GroupId, Members.GetData(), MemberCount);
			for (const CSteamID& Member : Members)
			{
				Group.Members.Add(ToSteamId(Member));
			}
		}
	}
#endif
}

// Runs on Steam's callback thread. Copy the payload, dispatch, return.
void FSteamFriendsBackend::OnPersonaStateChange(PersonaStateChange_t* Payload)
{
	const uint32 Flags = ToChangeFlags(static_cast<int32>(Payload->m_nChangeFlags));
	if (Flags == 0)
	{
		return; // Nicknames, chat and other noise.
	}

	const FSteamId UserId(static_cast<int64>(Payload->m_ulSteamID));
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [UserId, Flags](USteamFriendsSubsystem& Friends)
	{
		Friends.HandlePersonaChange(UserId, Flags);
	});
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

FSteamFriendsBackend::FSteamFriendsBackend(USteamFriendsSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

void FSteamFriendsBackend::CollectIds(ESteamFriendSource, TArray<FSteamId>&) const {}
bool FSteamFriendsBackend::ReadInfo(FSteamId Id, bool, FSteamFriendInfo& Out) const
{
	Out = FSteamFriendInfo();
	Out.SteamId = Id;
	Out.bInfoLoaded = false;
	return false;
}
int32 FSteamFriendsBackend::GetFriendCount() const { return 0; }
bool FSteamFriendsBackend::IsFriend(FSteamId) const { return false; }
void FSteamFriendsBackend::ReadGroups(TArray<FSteamFriendGroup>&) const {}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
