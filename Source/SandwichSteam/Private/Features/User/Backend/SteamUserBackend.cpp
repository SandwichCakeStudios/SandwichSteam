// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Features/User/Backend/SteamUserBackend.h"
#include "Features/User/SteamUserSubsystem.h"

#if SANDWICHSTEAM_WITH_STEAMWORKS

FSteamUserBackend::FSteamUserBackend(USteamUserSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
	PersonaStateChangeCallback.Register(this, &FSteamUserBackend::OnPersonaStateChange);
	AvatarImageLoadedCallback.Register(this, &FSteamUserBackend::OnAvatarImageLoaded);
#if SANDWICHSTEAM_WITH_WEBAPI_TICKET
	WebApiTicketCallback.Register(this, &FSteamUserBackend::OnWebApiTicketResponse);
#endif
}

bool FSteamUserBackend::IsLoggedOn() const
{
	ISteamUser* User = SteamUser();
	return User && User->BLoggedOn();
}

FSteamId FSteamUserBackend::GetLocalSteamId() const
{
	ISteamUser* User = SteamUser();
	return User ? FSteamId(static_cast<int64>(User->GetSteamID().ConvertToUint64())) : FSteamId();
}

FString FSteamUserBackend::GetPersonaName() const
{
	ISteamFriends* Friends = SteamFriends();
	return Friends ? FString(UTF8_TO_TCHAR(Friends->GetPersonaName())) : FString();
}

int32 FSteamUserBackend::GetSteamLevel() const
{
	ISteamUser* User = SteamUser();
	return User ? User->GetPlayerSteamLevel() : 0;
}

bool FSteamUserBackend::IsSubscribed() const
{
	ISteamApps* Apps = SteamApps();
	return Apps && Apps->BIsSubscribed();
}

bool FSteamUserBackend::IsSubscribedApp(uint32 AppId) const
{
	ISteamApps* Apps = SteamApps();
	return Apps && Apps->BIsSubscribedApp(AppId);
}

FSteamId FSteamUserBackend::GetAppOwner() const
{
	ISteamApps* Apps = SteamApps();
	return Apps ? FSteamId(static_cast<int64>(Apps->GetAppOwner().ConvertToUint64())) : FSteamId();
}

int64 FSteamUserBackend::GetEarliestPurchaseUnixTime(uint32 AppId) const
{
	ISteamApps* Apps = SteamApps();
	return Apps ? static_cast<int64>(Apps->GetEarliestPurchaseUnixTime(AppId)) : 0;
}

ESteamAvatarFetch FSteamUserBackend::FetchAvatar(FSteamId UserId, ESteamAvatarSize Size, FSteamAvatarPixels& OutPixels) const
{
	ISteamFriends* Friends = SteamFriends();
	ISteamUtils* Utils = SteamUtils();
	if (!Friends || !Utils || !UserId.IsValid())
	{
		return ESteamAvatarFetch::None;
	}

	const CSteamID SteamId(static_cast<uint64>(UserId.Value));

	// Non-friends only have an avatar once their persona data was requested. Returns true while it is being fetched
	// and false when the data is already available (always the case for friends and the local user).
	if (Friends->RequestUserInformation(SteamId, false))
	{
		return ESteamAvatarFetch::Loading;
	}

	int32 ImageHandle = 0;
	switch (Size)
	{
	case ESteamAvatarSize::Small:
		ImageHandle = Friends->GetSmallFriendAvatar(SteamId);
		break;
	case ESteamAvatarSize::Large:
		ImageHandle = Friends->GetLargeFriendAvatar(SteamId);
		break;
	case ESteamAvatarSize::Medium:
	default:
		ImageHandle = Friends->GetMediumFriendAvatar(SteamId);
		break;
	}

	if (ImageHandle == -1)
	{
		return ESteamAvatarFetch::Loading; // AvatarImageLoaded_t follows.
	}
	if (ImageHandle == 0)
	{
		return ESteamAvatarFetch::None;
	}

	uint32 Width = 0;
	uint32 Height = 0;
	if (!Utils->GetImageSize(ImageHandle, &Width, &Height) || Width == 0 || Height == 0)
	{
		return ESteamAvatarFetch::None;
	}

	const int32 ByteCount = static_cast<int32>(Width * Height * 4);
	OutPixels.Width = static_cast<int32>(Width);
	OutPixels.Height = static_cast<int32>(Height);
	OutPixels.Bgra.SetNumUninitialized(ByteCount);
	if (!Utils->GetImageRGBA(ImageHandle, OutPixels.Bgra.GetData(), ByteCount))
	{
		OutPixels = FSteamAvatarPixels();
		return ESteamAvatarFetch::None;
	}

	// Steam returns RGBA, the texture format is BGRA.
	uint8* Data = OutPixels.Bgra.GetData();
	for (int32 Index = 0; Index < ByteCount; Index += 4)
	{
		Swap(Data[Index], Data[Index + 2]);
	}

	return ESteamAvatarFetch::Ready;
}

uint32 FSteamUserBackend::RequestWebApiTicket(const FString& Identity) const
{
#if SANDWICHSTEAM_WITH_WEBAPI_TICKET
	ISteamUser* User = SteamUser();
	return User ? static_cast<uint32>(User->GetAuthTicketForWebApi(TCHAR_TO_UTF8(*Identity))) : 0;
#else
	return 0;
#endif
}

void FSteamUserBackend::CancelTicket(uint32 TicketHandle) const
{
	ISteamUser* User = SteamUser();
	if (User && TicketHandle != 0)
	{
		User->CancelAuthTicket(static_cast<HAuthTicket>(TicketHandle));
	}
}

// The three handlers below run on Steam's callback thread. Copy the payload, dispatch, return.

void FSteamUserBackend::OnPersonaStateChange(PersonaStateChange_t* Payload)
{
	const bool bNameChanged = (Payload->m_nChangeFlags & k_EPersonaChangeName) != 0;
	const bool bAvatarChanged = (Payload->m_nChangeFlags & k_EPersonaChangeAvatar) != 0;
	if (!bNameChanged && !bAvatarChanged)
	{
		return; // Presence and other changes are frequent and irrelevant here.
	}

	const FSteamId UserId(static_cast<int64>(Payload->m_ulSteamID));
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [UserId, bNameChanged, bAvatarChanged](USteamUserSubsystem& User)
	{
		User.HandlePersonaStateChange(UserId, bNameChanged, bAvatarChanged);
	});
}

void FSteamUserBackend::OnAvatarImageLoaded(AvatarImageLoaded_t* Payload)
{
	const FSteamId UserId(static_cast<int64>(Payload->m_steamID.ConvertToUint64()));
	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [UserId](USteamUserSubsystem& User)
	{
		User.HandleAvatarImageLoaded(UserId);
	});
}

#if SANDWICHSTEAM_WITH_WEBAPI_TICKET
void FSteamUserBackend::OnWebApiTicketResponse(GetTicketForWebApiResponse_t* Payload)
{
	const uint32 TicketHandle = static_cast<uint32>(Payload->m_hAuthTicket);
	const int32 NativeResult = static_cast<int32>(Payload->m_eResult);

	TArray<uint8> Ticket;
	if (Payload->m_cubTicket > 0)
	{
		Ticket.Append(Payload->m_rgubTicket, Payload->m_cubTicket);
	}

	SANDWICHSTEAM_DISPATCH(Dispatcher, Owner, [TicketHandle, NativeResult, Ticket = MoveTemp(Ticket)](USteamUserSubsystem& User) mutable
	{
		User.HandleWebApiTicketResponse(TicketHandle, NativeResult, MoveTemp(Ticket));
	});
}
#endif

#else // !SANDWICHSTEAM_WITH_STEAMWORKS

FSteamUserBackend::FSteamUserBackend(USteamUserSubsystem* InOwner, const TSharedRef<FSteamCallbackDispatcher, ESPMode::ThreadSafe>& InDispatcher)
	: Owner(InOwner)
	, Dispatcher(InDispatcher)
{
}

bool FSteamUserBackend::IsLoggedOn() const { return false; }
FSteamId FSteamUserBackend::GetLocalSteamId() const { return FSteamId(); }
FString FSteamUserBackend::GetPersonaName() const { return FString(); }
int32 FSteamUserBackend::GetSteamLevel() const { return 0; }
bool FSteamUserBackend::IsSubscribed() const { return false; }
bool FSteamUserBackend::IsSubscribedApp(uint32) const { return false; }
FSteamId FSteamUserBackend::GetAppOwner() const { return FSteamId(); }
int64 FSteamUserBackend::GetEarliestPurchaseUnixTime(uint32) const { return 0; }
ESteamAvatarFetch FSteamUserBackend::FetchAvatar(FSteamId, ESteamAvatarSize, FSteamAvatarPixels&) const { return ESteamAvatarFetch::None; }
uint32 FSteamUserBackend::RequestWebApiTicket(const FString&) const { return 0; }
void FSteamUserBackend::CancelTicket(uint32) const {}

#endif // SANDWICHSTEAM_WITH_STEAMWORKS
