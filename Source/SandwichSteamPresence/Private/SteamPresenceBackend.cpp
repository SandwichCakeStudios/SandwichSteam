// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamPresenceBackend.h"
#include "SteamPresenceSubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

FSteamPresenceBackend::FSteamPresenceBackend(USteamPresenceSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	FriendPresenceCallback.Register(this, &FSteamPresenceBackend::OnFriendRichPresenceUpdate);
}

int32 FSteamPresenceBackend::Apply(const FSteamPresenceBatch::FFlush& Flush, int32& OutCallCount) const
{
	OutCallCount = 0;
	ISteamFriends* Friends = SteamFriends();
	if (!Friends)
	{
		return 0;
	}

	int32 Refused = 0;
	if (Flush.bClearAll)
	{
		Friends->ClearRichPresence();
		++OutCallCount;
	}

	for (const FString& Key : Flush.Removes)
	{
		// An empty value removes the key.
		Refused += Friends->SetRichPresence(TCHAR_TO_UTF8(*Key), "") ? 0 : 1;
		++OutCallCount;
	}

	for (const TPair<FString, FString>& Pair : Flush.Sets)
	{
		Refused += Friends->SetRichPresence(TCHAR_TO_UTF8(*Pair.Key), TCHAR_TO_UTF8(*Pair.Value)) ? 0 : 1;
		++OutCallCount;
	}

	return Refused;
}

void FSteamPresenceBackend::ClearAll() const
{
	if (ISteamFriends* Friends = SteamFriends())
	{
		Friends->ClearRichPresence();
	}
}

FString FSteamPresenceBackend::GetFriendValue(FSteamId Friend, const FString& Key) const
{
	ISteamFriends* Friends = SteamFriends();
	if (!Friends || !Friend.IsValid())
	{
		return FString();
	}

	return FString(UTF8_TO_TCHAR(Friends->GetFriendRichPresence(CSteamID(static_cast<uint64>(Friend.Value)), TCHAR_TO_UTF8(*Key))));
}

void FSteamPresenceBackend::GetFriendValues(FSteamId Friend, TMap<FString, FString>& OutValues) const
{
	OutValues.Reset();
	ISteamFriends* Friends = SteamFriends();
	if (!Friends || !Friend.IsValid())
	{
		return;
	}

	const CSteamID SteamId(static_cast<uint64>(Friend.Value));
	const int32 KeyCount = Friends->GetFriendRichPresenceKeyCount(SteamId);
	for (int32 Index = 0; Index < KeyCount; ++Index)
	{
		const char* Key = Friends->GetFriendRichPresenceKeyByIndex(SteamId, Index);
		if (Key && Key[0] != '\0')
		{
			OutValues.Add(UTF8_TO_TCHAR(Key), UTF8_TO_TCHAR(Friends->GetFriendRichPresence(SteamId, Key)));
		}
	}
}

void FSteamPresenceBackend::RequestFriend(FSteamId Friend) const
{
	ISteamFriends* Friends = SteamFriends();
	if (Friends && Friend.IsValid())
	{
		Friends->RequestFriendRichPresence(CSteamID(static_cast<uint64>(Friend.Value)));
	}
}

// Runs on Steam's callback thread. Copy the payload, dispatch, return.
void FSteamPresenceBackend::OnFriendRichPresenceUpdate(FriendRichPresenceUpdate_t* Payload)
{
	const FSteamId FriendId(static_cast<int64>(Payload->m_steamIDFriend.ConvertToUint64()));
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [FriendId](USteamPresenceSubsystem& Presence)
	{
		Presence.HandleFriendPresenceUpdate(FriendId);
	});
}

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

FSteamPresenceBackend::FSteamPresenceBackend(USteamPresenceSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

int32 FSteamPresenceBackend::Apply(const FSteamPresenceBatch::FFlush&, int32& OutCallCount) const { OutCallCount = 0; return 0; }
void FSteamPresenceBackend::ClearAll() const {}
FString FSteamPresenceBackend::GetFriendValue(FSteamId, const FString&) const { return FString(); }
void FSteamPresenceBackend::GetFriendValues(FSteamId, TMap<FString, FString>& OutValues) const { OutValues.Reset(); }
void FSteamPresenceBackend::RequestFriend(FSteamId) const {}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
