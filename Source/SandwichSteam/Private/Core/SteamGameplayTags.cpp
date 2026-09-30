// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Core/SteamGameplayTags.h"

namespace SteamGameplayTags
{
	// Features
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_User, "Steam.Feature.User", "Steam user identity and avatars");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Utility, "Steam.Feature.Utility", "Steam utility (country, language, Steam Deck, text input)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Overlay, "Steam.Feature.Overlay", "Steam overlay");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Stats, "Steam.Feature.Stats", "Steam stats");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Achievements, "Steam.Feature.Achievements", "Steam achievements");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Leaderboards, "Steam.Feature.Leaderboards", "Steam leaderboards");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Friends, "Steam.Feature.Friends", "Steam friends");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Presence, "Steam.Feature.Presence", "Steam rich presence");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Sessions, "Steam.Feature.Sessions", "Steam sessions, lobbies and travel");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Voice, "Steam.Feature.Voice", "Steam in-session voice chat");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Input, "Steam.Feature.Input", "Steam Input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Cloud, "Steam.Feature.Cloud", "Steam cloud saves");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_DLC, "Steam.Feature.DLC", "Steam DLC");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Feature_Screenshots, "Steam.Feature.Screenshots", "Steam screenshots");

	// Errors
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Unavailable, "Steam.Error.Unavailable", "Steam or the Online Subsystem is not available");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_NotInitialized, "Steam.Error.NotInitialized", "Steam is not initialized yet");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_NotLoggedIn, "Steam.Error.NotLoggedIn", "No Steam user is logged in");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_FeatureDisabled, "Steam.Error.FeatureDisabled", "Feature is disabled in Sandwich Steam settings");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_NotSupported, "Steam.Error.NotSupported", "Operation is not supported");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_InvalidArgument, "Steam.Error.InvalidArgument", "Invalid argument");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Timeout, "Steam.Error.Timeout", "Steam request timed out");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Failed, "Steam.Error.Failed", "Steam request failed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Cancelled, "Steam.Error.Cancelled", "Request was cancelled (feature shut down or caller destroyed)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_RateLimited, "Steam.Error.RateLimited", "Steam rejected the request because of rate limiting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Offline, "Steam.Error.Offline", "Steam servers are not reachable (offline mode)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_QuotaExceeded, "Steam.Error.QuotaExceeded", "Steam Cloud quota exceeded");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_NotOwner, "Steam.Error.NotOwner", "Only the lobby or session owner can do this");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_WrongScope, "Steam.Error.WrongScope", "Operation is not available on this client/server type");

		// Sessions and lobbies
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Lobby_Full, "Steam.Error.Lobby.Full", "The lobby or session has no free slot");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Lobby_NotFound, "Steam.Error.Lobby.NotFound", "The lobby or session does not exist (any more)");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Lobby_Denied, "Steam.Error.Lobby.Denied", "Steam refused to let the user into the lobby (blocked, limited or not invited)");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Session_HostLeft, "Steam.Error.Session.HostLeft", "The connection to the host was lost or the host closed the session");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Session_Timeout, "Steam.Error.Session.Timeout", "The connection to the host timed out");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Error_Session_Kicked, "Steam.Error.Session.Kicked", "The host removed this player or refused the connection");
}
