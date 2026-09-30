// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/SteamInputRules.h"
#include "SteamInputRules.h"

using namespace SandwichSteam::Input;

namespace
{
	FActionSample Button(const TCHAR* Name, bool bDown, bool bActive = true)
	{
		FActionSample Sample;
		Sample.Action = FName(Name);
		Sample.Kind = ESteamInputActionKind::Button;
		Sample.bActive = bActive;
		Sample.bDown = bDown;
		return Sample;
	}

	FActionSample Analog(const TCHAR* Name, ESteamInputActionKind Kind, float X, float Y, bool bActive = true)
	{
		FActionSample Sample;
		Sample.Action = FName(Name);
		Sample.Kind = Kind;
		Sample.bActive = bActive;
		Sample.Value = FVector2f(X, Y);
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamInputNamesTest, "SandwichSteam.Input.Rules.Names",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamInputNamesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A plain name is fine"), CheckName(TEXT("Jump"), true) == ENameIssue::None);
	TestTrue(TEXT("Underscores and digits are fine"), CheckName(TEXT("Fire_2"), true) == ENameIssue::None);
	TestTrue(TEXT("Empty"), CheckName(FString(), true) == ENameIssue::Empty);
	TestTrue(TEXT("Digit first"), CheckName(TEXT("1Jump"), true) == ENameIssue::BadStart);
	TestTrue(TEXT("Underscore first"), CheckName(TEXT("_Jump"), true) == ENameIssue::BadStart);
	TestTrue(TEXT("A space"), CheckName(TEXT("Jump High"), true) == ENameIssue::BadCharacter);
	TestTrue(TEXT("A dash"), CheckName(TEXT("Jump-High"), true) == ENameIssue::BadCharacter);
	TestTrue(TEXT("Too long"), CheckName(FString::ChrN(65, TEXT('a')), true) == ENameIssue::TooLong);
	TestTrue(TEXT("The longest name is fine"), CheckName(FString::ChrN(64, TEXT('a')), true) == ENameIssue::None);
	TestTrue(TEXT("An action must not end in _X"), CheckName(TEXT("Move_X"), true) == ENameIssue::ReservedSuffix);
	TestTrue(TEXT("An action must not end in _Y"), CheckName(TEXT("Move_Y"), true) == ENameIssue::ReservedSuffix);
	TestTrue(TEXT("A set may end in _X"), CheckName(TEXT("Set_X"), false) == ENameIssue::None);
	TestTrue(TEXT("Lower case suffix is not reserved"), CheckName(TEXT("Move_x"), true) == ENameIssue::None);

	TestEqual(TEXT("Key name"), MakeKeyName(FName(TEXT("Jump"))), FName(TEXT("SteamInput_Jump")));
	TestEqual(TEXT("X axis key"), MakeAxisKeyName(FName(TEXT("Move")), false), FName(TEXT("SteamInput_Move_X")));
	TestEqual(TEXT("Y axis key"), MakeAxisKeyName(FName(TEXT("Move")), true), FName(TEXT("SteamInput_Move_Y")));

	FString Stripped;
	TestTrue(TEXT("Prefix is stripped"), StripKeyPrefix(FName(TEXT("SteamInput_Move_X")), Stripped) && Stripped == TEXT("Move_X"));
	TestFalse(TEXT("A gamepad key is not a Steam Input key"), StripKeyPrefix(FName(TEXT("Gamepad_FaceButton_Bottom")), Stripped));
	TestFalse(TEXT("The bare prefix is not a key"), StripKeyPrefix(FName(TEXT("SteamInput_")), Stripped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamInputSlotTableTest, "SandwichSteam.Input.Rules.SlotTable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamInputSlotTableTest::RunTest(const FString& Parameters)
{
	FSlotTable Table;
	TArray<int32> Added;
	TArray<int32> Removed;

	const TArray<uint64> None;
	Table.Update(None, 4, Added, Removed);
	TestTrue(TEXT("No controllers, no change"), Added.IsEmpty() && Removed.IsEmpty() && Table.NumConnected() == 0);

	// Two controllers connect.
	const TArray<uint64> Two = { 100, 200 };
	Table.Update(Two, 4, Added, Removed);
	TestTrue(TEXT("Both are added in order"), Added.Num() == 2 && Added[0] == 0 && Added[1] == 1 && Removed.IsEmpty());
	TestEqual(TEXT("First handle in slot 0"), Table.GetHandle(0), 100ull);
	TestEqual(TEXT("Second handle in slot 1"), Table.GetHandle(1), 200ull);
	TestEqual(TEXT("Find by handle"), Table.FindSlot(200), 1);

	// Nothing changed.
	Added.Reset();
	Table.Update(Two, 4, Added, Removed);
	TestTrue(TEXT("A steady state reports nothing"), Added.IsEmpty() && Removed.IsEmpty());

	// The first leaves: the second keeps its slot.
	const TArray<uint64> OnlySecond = { 200 };
	Table.Update(OnlySecond, 4, Added, Removed);
	TestTrue(TEXT("Slot 0 is removed"), Removed.Num() == 1 && Removed[0] == 0 && Added.IsEmpty());
	TestEqual(TEXT("The remaining controller keeps slot 1"), Table.FindSlot(200), 1);
	TestEqual(TEXT("Slot 0 is free"), Table.GetHandle(0), 0ull);
	TestEqual(TEXT("One connected"), Table.NumConnected(), 1);

	// A new controller takes the free slot 0.
	Removed.Reset();
	const TArray<uint64> SecondAndThird = { 200, 300 };
	Table.Update(SecondAndThird, 4, Added, Removed);
	TestTrue(TEXT("The new controller takes the lowest free slot"), Added.Num() == 1 && Added[0] == 0 && Table.GetHandle(0) == 300);

	// More controllers than slots: the extra one is ignored.
	Added.Reset();
	const TArray<uint64> Three = { 200, 300, 400 };
	Table.Update(Three, 2, Added, Removed);
	TestTrue(TEXT("The third controller has no slot"), Added.IsEmpty() && Table.FindSlot(400) == INDEX_NONE);

	// Everybody leaves.
	Table.Update(None, 4, Added, Removed);
	TestEqual(TEXT("All gone"), Table.NumConnected(), 0);
	TestEqual(TEXT("Nothing is left of the table"), Table.NumSlots(), 0);
	TestEqual(TEXT("A zero handle is never a slot"), Table.FindSlot(0), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamInputStateTrackerButtonTest, "SandwichSteam.Input.Rules.TrackerButtons",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamInputStateTrackerButtonTest::RunTest(const FString& Parameters)
{
	FStateTracker Tracker;
	TArray<FActionEvent> Events;

	// Idle.
	Tracker.Update(0, { Button(TEXT("Jump"), false) }, Events);
	TestEqual(TEXT("An idle button reports nothing"), Events.Num(), 0);

	// Press.
	Tracker.Update(0, { Button(TEXT("Jump"), true) }, Events);
	TestTrue(TEXT("Press is one event"), Events.Num() == 1 && Events[0].bDown && Events[0].Action == FName(TEXT("Jump")) && Events[0].Slot == 0);
	TestEqual(TEXT("Held"), Tracker.NumHeld(), 1);

	// Held: no repeat.
	Events.Reset();
	Tracker.Update(0, { Button(TEXT("Jump"), true) }, Events);
	TestEqual(TEXT("A held button is not reported again"), Events.Num(), 0);

	// Release.
	Tracker.Update(0, { Button(TEXT("Jump"), false) }, Events);
	TestTrue(TEXT("Release is one event"), Events.Num() == 1 && !Events[0].bDown);
	TestEqual(TEXT("Nothing held"), Tracker.NumHeld(), 0);

	// An action outside the active set reads as released even when its state says down.
	Events.Reset();
	Tracker.Update(0, { Button(TEXT("Jump"), true) }, Events);
	Events.Reset();
	Tracker.Update(0, { Button(TEXT("Jump"), true, /*bActive*/ false) }, Events);
	TestTrue(TEXT("An inactive action is released"), Events.Num() == 1 && !Events[0].bDown);

	// A held action that disappears from the samples (the set changed) is released.
	Events.Reset();
	Tracker.Update(0, { Button(TEXT("Jump"), true), Button(TEXT("Fire"), true) }, Events);
	Events.Reset();
	Tracker.Update(0, { Button(TEXT("Fire"), true) }, Events);
	TestTrue(TEXT("The missing action is released"), Events.Num() == 1 && Events[0].Action == FName(TEXT("Jump")) && !Events[0].bDown);

	// Slots are independent.
	Events.Reset();
	Tracker.Update(1, { Button(TEXT("Fire"), true) }, Events);
	TestTrue(TEXT("Same action on another slot is a new press"), Events.Num() == 1 && Events[0].Slot == 1 && Events[0].bDown);

	// A controller leaves: everything on its slot is released.
	Events.Reset();
	Tracker.ReleaseSlot(1, Events);
	TestTrue(TEXT("The slot is released"), Events.Num() == 1 && Events[0].Slot == 1 && !Events[0].bDown);
	Events.Reset();
	Tracker.ReleaseSlot(1, Events);
	TestEqual(TEXT("Releasing twice reports nothing"), Events.Num(), 0);

	// Release all.
	Events.Reset();
	Tracker.ReleaseAll(Events);
	TestTrue(TEXT("Slot 0 Fire is released"), Events.Num() == 1 && Events[0].Action == FName(TEXT("Fire")));
	TestEqual(TEXT("Nothing held any more"), Tracker.NumHeld(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamInputStateTrackerAnalogTest, "SandwichSteam.Input.Rules.TrackerAnalog",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FSteamInputStateTrackerAnalogTest::RunTest(const FString& Parameters)
{
	FStateTracker Tracker;
	TArray<FActionEvent> Events;

	// A stick at rest reports nothing.
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.0f, 0.0f) }, Events);
	TestEqual(TEXT("A stick at rest is silent"), Events.Num(), 0);

	// It moves.
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.5f, -0.25f) }, Events);
	TestTrue(TEXT("The first movement is reported"), Events.Num() == 1 && FMath::IsNearlyEqual(Events[0].Value.X, 0.5f) && FMath::IsNearlyEqual(Events[0].Value.Y, -0.25f));

	// The same value again, and a value within the epsilon: silent.
	Events.Reset();
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.5f, -0.25f) }, Events);
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.5001f, -0.25f) }, Events);
	TestEqual(TEXT("An unchanged stick is silent"), Events.Num(), 0);

	// A real change.
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.75f, -0.25f) }, Events);
	TestTrue(TEXT("A change is reported"), Events.Num() == 1 && FMath::IsNearlyEqual(Events[0].Value.X, 0.75f));

	// The return to zero is always reported (a stuck axis would move the character forever).
	Events.Reset();
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.0f, 0.0f) }, Events);
	TestTrue(TEXT("Zero is reported"), Events.Num() == 1 && !Events[0].bDown && Events[0].Value.IsZero());
	TestEqual(TEXT("Nothing held"), Tracker.NumHeld(), 0);

	// Only Y moved.
	Events.Reset();
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.0f, 0.4f) }, Events);
	TestTrue(TEXT("A single axis counts"), Events.Num() == 1 && FMath::IsNearlyEqual(Events[0].Value.Y, 0.4f));

	// A trigger only has X, clamped to 0..1.
	Events.Reset();
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.0f, 0.4f), Analog(TEXT("Fire"), ESteamInputActionKind::Trigger, 1.5f, 0.9f) }, Events);
	TestTrue(TEXT("A trigger reports X clamped"), Events.Num() == 1 && Events[0].Action == FName(TEXT("Fire")) && FMath::IsNearlyEqual(Events[0].Value.X, 1.0f) && Events[0].Value.Y == 0.0f);

	// The set changed: the stick and the trigger vanish from the samples and are released.
	Events.Reset();
	Tracker.Update(0, {}, Events);
	TestEqual(TEXT("Both are released"), Events.Num(), 2);
	for (const FActionEvent& Event : Events)
	{
		TestTrue(TEXT("Released analog values are zero"), !Event.bDown && Event.Value.IsZero());
	}

	// An inactive analog action reads as zero.
	Events.Reset();
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.5f, 0.5f) }, Events);
	Events.Reset();
	Tracker.Update(0, { Analog(TEXT("Move"), ESteamInputActionKind::StickPad, 0.5f, 0.5f, /*bActive*/ false) }, Events);
	TestTrue(TEXT("An inactive stick is released"), Events.Num() == 1 && Events[0].Value.IsZero());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
