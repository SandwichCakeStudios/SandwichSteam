// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Input/SteamInputVdfWriter.h"

namespace
{
	FSteamInputVdfAction MakeAction(const TCHAR* Name, ESteamInputActionKind Kind, const TCHAR* Mode = TEXT(""), const TCHAR* Title = TEXT(""))
	{
		FSteamInputVdfAction Action;
		Action.Name = Name;
		Action.Kind = Kind;
		Action.InputMode = Mode;
		Action.Title = Title;
		return Action;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSteamInputVdfGoldenTest, "SandwichSteam.Editor.InputVdf.Golden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSteamInputVdfGoldenTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Nothing to write gives an empty file"), FSteamInputVdfWriter::Build(TArray<FSteamInputVdfSet>()).IsEmpty());

	FSteamInputVdfSet Nameless;
	TestTrue(TEXT("A set without a name is skipped"), FSteamInputVdfWriter::Build({ Nameless }).IsEmpty());

	FSteamInputVdfSet Gameplay;
	Gameplay.Name = TEXT("Gameplay");
	Gameplay.Title = TEXT("Gameplay Controls");
	Gameplay.Actions.Add(MakeAction(TEXT("Move"), ESteamInputActionKind::StickPad));
	Gameplay.Actions.Add(MakeAction(TEXT("Fire"), ESteamInputActionKind::Trigger));
	Gameplay.Actions.Add(MakeAction(TEXT("Jump"), ESteamInputActionKind::Button, TEXT(""), TEXT("Jump up")));
	Gameplay.Actions.Add(MakeAction(TEXT(""), ESteamInputActionKind::Button)); // Skipped.

	FSteamInputVdfSet Vehicle;
	Vehicle.Name = TEXT("Vehicle");
	Vehicle.bLayer = true;
	Vehicle.Actions.Add(MakeAction(TEXT("Steer"), ESteamInputActionKind::StickPad, TEXT("joystick_camera")));
	Vehicle.Actions.Add(MakeAction(TEXT("Jump"), ESteamInputActionKind::Button));

	const FString Expected =
		TEXT("\"In Game Actions\"\n")
		TEXT("{\n")
		TEXT("\t\"actions\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"Gameplay\"\n")
		TEXT("\t\t{\n")
		TEXT("\t\t\t\"title\"\t\"#Set_Gameplay\"\n")
		TEXT("\t\t\t\"StickPadGyro\"\n")
		TEXT("\t\t\t{\n")
		TEXT("\t\t\t\t\"Move\"\n")
		TEXT("\t\t\t\t{\n")
		TEXT("\t\t\t\t\t\"title\"\t\"#Action_Move\"\n")
		TEXT("\t\t\t\t\t\"input_mode\"\t\"joystick_move\"\n")
		TEXT("\t\t\t\t}\n")
		TEXT("\t\t\t}\n")
		TEXT("\t\t\t\"AnalogTrigger\"\n")
		TEXT("\t\t\t{\n")
		TEXT("\t\t\t\t\"Fire\"\t\"#Action_Fire\"\n")
		TEXT("\t\t\t}\n")
		TEXT("\t\t\t\"Button\"\n")
		TEXT("\t\t\t{\n")
		TEXT("\t\t\t\t\"Jump\"\t\"#Action_Jump\"\n")
		TEXT("\t\t\t}\n")
		TEXT("\t\t}\n")
		TEXT("\t}\n")
		TEXT("\t\"action_layers\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"Vehicle\"\n")
		TEXT("\t\t{\n")
		TEXT("\t\t\t\"title\"\t\"#Set_Vehicle\"\n")
		TEXT("\t\t\t\"legacy_set\"\t\"1\"\n")
		TEXT("\t\t\t\"StickPadGyro\"\n")
		TEXT("\t\t\t{\n")
		TEXT("\t\t\t\t\"Steer\"\n")
		TEXT("\t\t\t\t{\n")
		TEXT("\t\t\t\t\t\"title\"\t\"#Action_Steer\"\n")
		TEXT("\t\t\t\t\t\"input_mode\"\t\"joystick_camera\"\n")
		TEXT("\t\t\t\t}\n")
		TEXT("\t\t\t}\n")
		TEXT("\t\t\t\"Button\"\n")
		TEXT("\t\t\t{\n")
		TEXT("\t\t\t\t\"Jump\"\t\"#Action_Jump\"\n")
		TEXT("\t\t\t}\n")
		TEXT("\t\t}\n")
		TEXT("\t}\n")
		TEXT("\t\"localization\"\n")
		TEXT("\t{\n")
		TEXT("\t\t\"english\"\n")
		TEXT("\t\t{\n")
		TEXT("\t\t\t\"Set_Gameplay\"\t\"Gameplay Controls\"\n")
		TEXT("\t\t\t\"Action_Move\"\t\"Move\"\n")
		TEXT("\t\t\t\"Action_Fire\"\t\"Fire\"\n")
		TEXT("\t\t\t\"Action_Jump\"\t\"Jump up\"\n")
		TEXT("\t\t\t\"Set_Vehicle\"\t\"Vehicle\"\n")
		TEXT("\t\t\t\"Action_Steer\"\t\"Steer\"\n")
		TEXT("\t\t}\n")
		TEXT("\t}\n")
		TEXT("}\n");

	TArray<FSteamInputVdfSet> Sets;
	Sets.Add(Gameplay);
	Sets.Add(Vehicle);
	const FString Actual = FSteamInputVdfWriter::Build(Sets);
	TestEqual(TEXT("Golden action file"), Actual, Expected);

	// Only a layer: no "actions" section.
	TArray<FSteamInputVdfSet> OnlyLayer;
	OnlyLayer.Add(Vehicle);
	const FString LayerFile = FSteamInputVdfWriter::Build(OnlyLayer);
	TestTrue(TEXT("A layer alone has no actions section"), !LayerFile.Contains(TEXT("\"actions\"")) && LayerFile.Contains(TEXT("\"action_layers\"")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
