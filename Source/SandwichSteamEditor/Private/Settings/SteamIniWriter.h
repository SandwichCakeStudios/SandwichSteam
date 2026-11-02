// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * One key=value line that must exist in an ini section.
 * A key that starts with '+' (an array element, for example +NetDriverDefinitions) is matched by key AND value: the line is
 * added when no line has both, and other lines with the same key are left alone.
 * With bRemove the line must NOT exist: every matching line is deleted (same matching rules), nothing else is touched.
 */
struct FSteamIniEntry
{
	FString Section;
	FString Key;
	FString Value;
	bool bRemove = false;
};

enum class ESteamIniChange : uint8
{
	/** Key already has the wanted value. */
	Unchanged,
	/** Key (or its whole section) is missing and will be added. */
	Added,
	/** Key exists with another value and will be replaced. */
	Modified,
	/** A bRemove entry whose line exists and will be deleted. */
	Removed
};

struct FSteamIniChange
{
	FSteamIniEntry Entry;
	ESteamIniChange Change = ESteamIniChange::Unchanged;
	/** Previous value when Change is Modified. */
	FString OldValue;
};

/** The values behind the Advanced page's network tuning (see USteamToolSettings, Networking category). */
struct FSteamNetworkTuning
{
	/** Bytes per second per client: IpNetDriver rates, Player speeds and MaxDynamicBandwidth. */
	int32 BandwidthPerClient = 200000;
	/** GameNetworkManager TotalNetBandwidth, shared by all connections. */
	int32 TotalBandwidth = 800000;
	/** GameNetworkManager MinDynamicBandwidth. */
	int32 MinDynamicBandwidth = 20000;
	/** IpNetDriver InitialConnectTimeout in seconds. */
	float InitialConnectTimeout = 60.0f;
};

/**
 * Pure ini text editing (string in, string out, no file or engine access) so it can be unit tested.
 * Idempotent: Apply(Apply(Ini, E), E) == Apply(Ini, E), and Apply returns the input untouched when nothing changes.
 * Existing content, comments, ordering and the line ending style are preserved.
 */
class FSteamIniWriter
{
public:
	/**
	 * The DefaultEngine.ini entries Sandwich Steam needs for the given Steam App ID.
	 * With bWithSessions the SteamSockets net driver entries of the Sessions module are included.
	 * With bWithRelaunchOff [OnlineSubsystemSteam] bRelaunchInSteam=false is included (USteamToolSettings::bWriteRelaunchInSteamOff).
	 */
	static TArray<FSteamIniEntry> BuildRequiredEntries(int32 SteamAppId, bool bWithSessions = false, bool bWithRelaunchOff = true);

	/**
	 * The DefaultEngine.ini entries that switch on the Online Subsystem voice ([OnlineSubsystem] bHasVoiceEnabled and [Voice] bEnabled).
	 * Append only when the Voice module is installed. bOssVoice false (the Steam voice pipeline) makes them bRemove entries, because the
	 * two paths would both read the microphone.
	 */
	static TArray<FSteamIniEntry> BuildVoiceEngineEntries(bool bOssVoice);

	/**
	 * The DefaultGame.ini entry of the Voice module: the game session asks for push to talk ([/Script/Engine.GameSession]
	 * bRequiresPushToTalk=true), so the engine never opens the microphone by itself (the Voice module opens it, also in open mic mode).
	 * Append when the Voice module is installed. Only the Online Subsystem voice needs it: bOssVoice false returns nothing, so the
	 * Steam voice pipeline leaves DefaultGame.ini alone (an existing line is not removed; the pipeline ignores it).
	 */
	static TArray<FSteamIniEntry> BuildVoiceGameEntries(bool bOssVoice);

	/**
	 * The DefaultEngine.ini network tuning entries: IpNetDriver MaxClientRate, MaxInternetClientRate and InitialConnectTimeout, and
	 * Player ConfiguredInternetSpeed and ConfiguredLanSpeed (the client enforces its own limit, so it has to match the server's).
	 * SteamSocketsNetDriver reads the IpNetDriver section. Append to BuildRequiredEntries only when USteamToolSettings::bWriteNetworkTuning is on.
	 */
	static TArray<FSteamIniEntry> BuildNetworkEngineEntries(const FSteamNetworkTuning& Tuning);

	/**
	 * The DefaultEngine.ini entry that loads the engine's SteamAuth packet handler component ([PacketHandlerComponents]
	 * +Components=OnlineSubsystemSteam.SteamAuthComponentModuleInterface). Append to BuildRequiredEntries only when the Auth module is
	 * installed. bEnable false makes it a bRemove entry, so turning the setting off takes the line out again.
	 * Other +Components lines (encryption etc.) are never touched.
	 */
	static TArray<FSteamIniEntry> BuildSteamAuthEntries(bool bEnable = true);

	/** The DefaultGame.ini network tuning entries: GameNetworkManager TotalNetBandwidth, MaxDynamicBandwidth and MinDynamicBandwidth. */
	static TArray<FSteamIniEntry> BuildNetworkGameEntries(const FSteamNetworkTuning& Tuning);

	/** False (with a short reason in OutProblem) when the values contradict each other: Min above per-client, or per-client above the total. */
	static bool IsNetworkTuningConsistent(const FSteamNetworkTuning& Tuning, FString& OutProblem);

	/** What Apply would do, per entry. */
	static TArray<FSteamIniChange> Diff(const FString& Ini, const TArray<FSteamIniEntry>& Entries);

	/**
	 * Returns Ini with every entry set. Missing sections are appended, missing keys added at the end of their section.
	 * bRemove entries delete their lines; a section left empty keeps its header.
	 */
	static FString Apply(const FString& Ini, const TArray<FSteamIniEntry>& Entries);
};
