// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamFriendsLibrary.h"
#include "Core/SteamGameplayTags.h"
#include "SteamFriendsSubsystem.h"

namespace
{
	FSteamResult NotAvailable()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_FeatureDisabled, NSLOCTEXT("SandwichSteam", "FriendsLibNotAvailable", "The Friends feature is not available in this game instance."));
	}
}

int32 USteamFriendsLibrary::GetSteamFriendCount(const UObject* WorldContextObject)
{
	const USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends ? Friends->GetFriendCount() : 0;
}

bool USteamFriendsLibrary::IsSteamFriend(const UObject* WorldContextObject, FSteamId User)
{
	const USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends && Friends->IsFriend(User);
}

bool USteamFriendsLibrary::GetSteamFriendInfo(const UObject* WorldContextObject, FSteamId User, FSteamFriendInfo& Info)
{
	const USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	if (!Friends)
	{
		Info = FSteamFriendInfo();
		return false;
	}
	return Friends->GetFriendInfo(User, Info);
}

TArray<FSteamFriendGroup> USteamFriendsLibrary::GetSteamFriendGroups(const UObject* WorldContextObject)
{
	const USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends ? Friends->GetFriendGroups() : TArray<FSteamFriendGroup>();
}

bool USteamFriendsLibrary::HasSteamFriendChange(int32 ChangeFlags, ESteamFriendChange Change)
{
	return (ChangeFlags & static_cast<int32>(Change)) != 0;
}

FSteamResult USteamFriendsLibrary::OpenSteamFriendsList(const UObject* WorldContextObject)
{
	USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends ? Friends->OpenFriendsList() : NotAvailable();
}

FSteamResult USteamFriendsLibrary::OpenSteamProfile(const UObject* WorldContextObject, FSteamId User)
{
	USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends ? Friends->OpenProfile(User) : NotAvailable();
}

FSteamResult USteamFriendsLibrary::OpenSteamChat(const UObject* WorldContextObject, FSteamId User)
{
	USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends ? Friends->OpenChat(User) : NotAvailable();
}

FSteamResult USteamFriendsLibrary::OpenSteamAddFriend(const UObject* WorldContextObject, FSteamId User)
{
	USteamFriendsSubsystem* Friends = USteamFriendsSubsystem::Get(WorldContextObject);
	return Friends ? Friends->OpenAddFriend(User) : NotAvailable();
}
