// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SteamVoiceRules.h"

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
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamVoiceMuteBookTest, "SandwichSteam.Voice.Rules.MuteBook",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

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

#endif // WITH_DEV_AUTOMATION_TESTS
