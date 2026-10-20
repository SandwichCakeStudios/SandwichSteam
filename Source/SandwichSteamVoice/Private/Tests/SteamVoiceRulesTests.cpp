// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SteamEngineCompat.h"
#include "SteamVoiceRules.h"
#include "SteamVoiceTypes.h"

using namespace SandwichSteam::Voice;

namespace
{
	TSet<int64> MakeSet(std::initializer_list<int64> Ids)
	{
		TSet<int64> Set;
		for (const int64 Id : Ids)
		{
			Set.Add(Id);
		}
		return Set;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceTalkingTrackerTest, "SandwichSteam.Voice.Rules.TalkingTracker",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceTalkingTrackerTest::RunTest(const FString& Parameters)
{
	FTalkingTracker Tracker;
	TArray<FTalkingChange> Changes;

	// Nobody talks: nothing to report.
	Tracker.Update(MakeSet({}), MakeSet({ 1, 2 }), Changes);
	TestEqual(TEXT("Silence reports nothing"), Changes.Num(), 0);

	// Player 1 starts.
	Tracker.Update(MakeSet({ 1 }), MakeSet({ 1, 2 }), Changes);
	TestTrue(TEXT("Start reported once"), Changes.Num() == 1 && Changes[0].Player == 1 && Changes[0].bTalking);
	TestTrue(TEXT("Player 1 talks"), Tracker.IsTalking(1));

	// Still talking: no repeat.
	Changes.Reset();
	Tracker.Update(MakeSet({ 1 }), MakeSet({ 1, 2 }), Changes);
	TestEqual(TEXT("A steady talker is not reported again"), Changes.Num(), 0);

	// 1 stops and 2 starts in one poll: stop first, then start.
	Tracker.Update(MakeSet({ 2 }), MakeSet({ 1, 2 }), Changes);
	TestTrue(TEXT("Stop then start"), Changes.Num() == 2 && Changes[0].Player == 1 && !Changes[0].bTalking && Changes[1].Player == 2 && Changes[1].bTalking);

	// 2 leaves the game while the backend still says talking: reported as stopped, and a talker who is not present never starts.
	Changes.Reset();
	Tracker.Update(MakeSet({ 2, 3 }), MakeSet({ 1 }), Changes);
	TestTrue(TEXT("A player who left stops"), Changes.Num() == 1 && Changes[0].Player == 2 && !Changes[0].bTalking);
	TestFalse(TEXT("An absent talker is ignored"), Tracker.IsTalking(3));

	// Clear reports every talker as stopped.
	Tracker.Update(MakeSet({ 1 }), MakeSet({ 1 }), Changes);
	Changes.Reset();
	Tracker.Clear(Changes);
	TestTrue(TEXT("Clear stops talkers"), Changes.Num() == 1 && Changes[0].Player == 1 && !Changes[0].bTalking);
	TestEqual(TEXT("Nobody talks after Clear"), Tracker.GetTalking().Num(), 0);

	// Event-driven: Set reports only real changes.
	TestTrue(TEXT("Set start changes"), Tracker.Set(4, true));
	TestFalse(TEXT("Set start twice changes nothing"), Tracker.Set(4, true));
	TestTrue(TEXT("Set start is talking"), Tracker.IsTalking(4));
	TestTrue(TEXT("Set stop changes"), Tracker.Set(4, false));
	TestFalse(TEXT("Set stop of a silent player changes nothing"), Tracker.Set(4, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceMuteBookTest, "SandwichSteam.Voice.Rules.MuteBook",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceMuteBookTest::RunTest(const FString& Parameters)
{
	FMuteBook Book;
	TestFalse(TEXT("Unknown player is not muted"), Book.IsMuted(7));
	TestFalse(TEXT("Unknown player needs nothing"), Book.NeedsApply(7));

	// A game mute needs applying until the backend confirms it.
	TestTrue(TEXT("Mute changed the state"), Book.SetUserMuted(7, true, false));
	TestFalse(TEXT("Muting twice changes nothing"), Book.SetUserMuted(7, true, false));
	TestTrue(TEXT("Mute must be applied"), Book.NeedsApply(7));
	Book.MarkApplied(7);
	TestFalse(TEXT("Applied mute needs nothing"), Book.NeedsApply(7));

	// A new session registers the talkers again.
	Book.ForgetApplied();
	TestTrue(TEXT("Mute is kept, but must be applied again"), Book.IsMuted(7) && Book.NeedsApply(7));
	Book.MarkApplied(7);

	// Two sources: unmuting the game mute keeps an automatic (blocked) mute.
	TestFalse(TEXT("Adding the auto mute changes nothing, the player was muted already"), Book.SetAutoMuted(7, true));
	TestFalse(TEXT("Removing the game mute keeps the player muted"), Book.SetUserMuted(7, false, false));
	TestTrue(TEXT("Still muted"), Book.IsMuted(7) && Book.IsAutoMuted(7) && !Book.IsUserMuted(7));
	TestFalse(TEXT("Nothing to send: the backend already has the mute"), Book.NeedsApply(7));

	// Removing the last source unmutes and must be applied.
	TestTrue(TEXT("Removing the auto mute unmutes"), Book.SetAutoMuted(7, false));
	TestTrue(TEXT("The unmute must be applied"), Book.NeedsApply(7));
	Book.MarkApplied(7);
	TestFalse(TEXT("Unmute applied"), Book.NeedsApply(7));

	// System wide flag follows the game mute.
	Book.SetUserMuted(9, true, true);
	TestTrue(TEXT("System wide is remembered"), Book.IsSystemWide(9));
	Book.SetUserMuted(9, false, false);
	TestFalse(TEXT("System wide is dropped with the mute"), Book.IsSystemWide(9));

	// A mute set before the player exists is listed.
	Book.SetAutoMuted(11, true);
	TestEqual(TEXT("Muted list"), Book.GetMutedPlayers().Num(), 1);
	Book.Reset();
	TestEqual(TEXT("Reset clears everything"), Book.GetMutedPlayers().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceChannelResolveTest, "SandwichSteam.Voice.Rules.ChannelResolve",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceChannelResolveTest::RunTest(const FString& Parameters)
{
	TMap<FGameplayTag, int32> TagMap;

	// Id channels need no map.
	FSteamVoiceChannel Channel;
	TestEqual(TEXT("Unset channel"), Channel.Resolve(TagMap), static_cast<int32>(INDEX_NONE));
	Channel.Id = 3;
	TestEqual(TEXT("Id channel"), Channel.Resolve(TagMap), 3);
	Channel.Id = -5;
	TestEqual(TEXT("Negative id"), Channel.Resolve(TagMap), static_cast<int32>(INDEX_NONE));

	// The root tag is registered by the plugin's tag ini; any valid tag shows the rules.
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Steam.Voice.Channel"), false);
	if (!Tag.IsValid())
	{
		AddWarning(TEXT("Steam.Voice.Channel is not registered; tag cases skipped."));
		return true;
	}

	Channel.Tag = Tag;
	Channel.Id = 3;
	TestEqual(TEXT("Unmapped tag does not fall back to the id"), Channel.Resolve(TagMap), static_cast<int32>(INDEX_NONE));
	TagMap.Add(Tag, 12);
	TestEqual(TEXT("Mapped tag wins over the id"), Channel.Resolve(TagMap), 12);
	TagMap.Add(Tag, -1);
	TestEqual(TEXT("Negative mapped id"), Channel.Resolve(TagMap), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceRouteFilterTest, "SandwichSteam.Voice.Rules.RouteFilter",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceRouteFilterTest::RunTest(const FString& Parameters)
{
	const int32 Sent[] = { 1, 2, 5 };
	FVoiceRoute Route;
	FVoiceRoute Out;

	// Default rules pass everything.
	const FPlayerRules Open;
	FilterForTalker(Open, true, Sent, Route);
	TestTrue(TEXT("Default talker keeps mouth and channels"), Route.bMouth && Route.Channels.Num() == 3);
	FilterForListener(Open, true, Route, Out);
	TestTrue(TEXT("Default listener gets everything"), Out == Route);

	// Server mute and identity conflict send nothing.
	FPlayerRules Muted;
	Muted.bCanTalk = false;
	FilterForTalker(Muted, true, Sent, Route);
	TestTrue(TEXT("Server muted talker sends nothing"), Route.IsEmpty());
	FPlayerRules Refused;
	Refused.bRefused = true;
	FilterForTalker(Refused, true, Sent, Route);
	TestTrue(TEXT("Refused talker sends nothing"), Route.IsEmpty());

	// Dead chat: no mouth, transmit only on channel 5.
	FPlayerRules Dead;
	Dead.bOwnerVoice = false;
	Dead.Transmit.bRestricted = true;
	Dead.Transmit.Allowed = { 5 };
	FilterForTalker(Dead, true, Sent, Route);
	TestTrue(TEXT("Dead talker: no mouth, channel 5 only"), !Route.bMouth && Route.Channels.Num() == 1 && Route.Channels[0] == 5);

	// Restricted with an empty list: no channel at all.
	FPlayerRules NoChannels;
	NoChannels.Transmit.bRestricted = true;
	FilterForTalker(NoChannels, true, Sent, Route);
	TestTrue(TEXT("Restricted empty transmit keeps only the mouth"), Route.bMouth && Route.Channels.IsEmpty());

	// Listener side: only allowed channels, mouth only in range.
	FilterForTalker(Open, true, Sent, Route);
	FPlayerRules Alive;
	Alive.Listen.bRestricted = true;
	Alive.Listen.Allowed = { 1, 2 };
	FilterForListener(Alive, true, Route, Out);
	TestTrue(TEXT("Alive listener does not hear dead chat"), Out.bMouth && Out.Channels.Num() == 2 && !Out.Channels.Contains(5));
	FilterForListener(Alive, false, Route, Out);
	TestTrue(TEXT("Out of range: channels only"), !Out.bMouth && Out.Channels.Num() == 2);

	// Nothing left for one listener: the sender skips them.
	FilterForTalker(Dead, true, Sent, Route);
	FilterForListener(Alive, true, Route, Out);
	TestTrue(TEXT("Dead talker to alive listener: nothing"), Out.IsEmpty());

	// A refused listener gets nothing.
	FilterForTalker(Open, true, Sent, Route);
	FilterForListener(Refused, true, Route, Out);
	TestTrue(TEXT("Refused listener gets nothing"), Out.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceOwnerDistanceTest, "SandwichSteam.Voice.Rules.OwnerDistance",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceOwnerDistanceTest::RunTest(const FString& Parameters)
{
	// Talker override wins, below 0 means no limit, 0 takes the global value.
	TestEqual(TEXT("Global"), ResolveOwnerVoiceDistance(1000.0f, 0.0f), 1000.0f);
	TestEqual(TEXT("Override"), ResolveOwnerVoiceDistance(1000.0f, 3000.0f), 3000.0f);
	TestEqual(TEXT("Override without global"), ResolveOwnerVoiceDistance(0.0f, 500.0f), 500.0f);
	TestEqual(TEXT("Negative override = no limit"), ResolveOwnerVoiceDistance(1000.0f, -1.0f), 0.0f);
	TestEqual(TEXT("Negative global is off"), ResolveOwnerVoiceDistance(-5.0f, 0.0f), 0.0f);

	const auto Sq = [](double Distance) { return Distance * Distance; };

	// No limit: always in range, even without locations.
	TestTrue(TEXT("No limit, unknown"), UpdateMouthInRange(false, false, 0.0, 0.0f));
	TestTrue(TEXT("No limit, receive"), IsMouthAcceptable(false, 0.0, 0.0f));

	// Unknown location with a limit: out.
	TestFalse(TEXT("Unknown is out"), UpdateMouthInRange(true, false, 0.0, 1000.0f));
	TestFalse(TEXT("Unknown is dropped"), IsMouthAcceptable(false, 0.0, 1000.0f));

	// Hysteresis: enter at the distance, leave beyond 110 %.
	TestTrue(TEXT("Enter at the distance"), UpdateMouthInRange(false, true, Sq(1000.0), 1000.0f));
	TestFalse(TEXT("Not entered just beyond"), UpdateMouthInRange(false, true, Sq(1050.0), 1000.0f));
	TestTrue(TEXT("Stays in within the margin"), UpdateMouthInRange(true, true, Sq(1050.0), 1000.0f));
	TestFalse(TEXT("Leaves beyond the margin"), UpdateMouthInRange(true, true, Sq(1101.0), 1000.0f));

	// Receiver: wider margin than the sender, so honest edge packets still play.
	TestTrue(TEXT("Receiver keeps the sender's edge"), IsMouthAcceptable(true, Sq(1100.0), 1000.0f));
	TestFalse(TEXT("Receiver drops beyond 120 %"), IsMouthAcceptable(true, Sq(1201.0), 1000.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceIdentityTest, "SandwichSteam.Voice.Rules.IdentityAndMapHash",
	SANDWICHSTEAM_TEST_CONTEXT_MASK | EAutomationTestFlags::ProductFilter)

bool FSteamVoiceIdentityTest::RunTest(const FString& Parameters)
{
	TSet<int64> Duplicates;
	const int64 Unique[] = { 1, 2, 3 };
	FindDuplicateIds(Unique, Duplicates);
	TestEqual(TEXT("No duplicates"), Duplicates.Num(), 0);
	const int64 Twice[] = { 4, 1, 4, 2, 4, 1 };
	FindDuplicateIds(Twice, Duplicates);
	TestTrue(TEXT("Duplicates found once each"), Duplicates.Num() == 2 && Duplicates.Contains(4) && Duplicates.Contains(1));

	TMap<FGameplayTag, int32> Map;
	TestTrue(TEXT("Empty map hashes to 0"), HashChannelMap(Map) == 0);

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Steam.Voice.Channel"), false);
	if (!Tag.IsValid())
	{
		AddWarning(TEXT("Steam.Voice.Channel is not registered; map hash cases skipped."));
		return true;
	}
	Map.Add(Tag, 3);
	const uint32 Hash = HashChannelMap(Map);
	TestTrue(TEXT("A map hashes to something"), Hash != 0);
	TestTrue(TEXT("Same map, same hash"), HashChannelMap(TMap<FGameplayTag, int32>(Map)) == Hash);
	Map.Add(Tag, 4);
	TestTrue(TEXT("Another id, another hash"), HashChannelMap(Map) != Hash);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
