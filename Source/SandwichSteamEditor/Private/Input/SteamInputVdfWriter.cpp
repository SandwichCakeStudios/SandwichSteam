// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "Input/SteamInputVdfWriter.h"
#include "Publish/SteamVdfWriter.h"

namespace
{
	FString Line(int32 Depth, const FString& Text)
	{
		return FString::ChrN(Depth, TEXT('\t')) + Text + TEXT("\n");
	}

	FString Pair(int32 Depth, const FString& Key, const FString& Value)
	{
		return Line(Depth, FString::Printf(TEXT("%s\t%s"), *FSteamVdfWriter::Quote(Key), *FSteamVdfWriter::Quote(Value)));
	}

	/** One set or layer: title, then the action groups. */
	FString BuildSet(const FSteamInputVdfSet& Set, bool bLayer)
	{
		FString Out;
		Out += Line(2, FSteamVdfWriter::Quote(Set.Name));
		Out += Line(2, TEXT("{"));
		Out += Pair(3, TEXT("title"), TEXT("#") + FSteamInputVdfWriter::GetSetKey(Set.Name));
		if (bLayer)
		{
			Out += Pair(3, TEXT("legacy_set"), TEXT("1"));
		}

		const auto WriteGroup = [&Out, &Set](const TCHAR* Group, ESteamInputActionKind Kind)
		{
			FString Body;
			for (const FSteamInputVdfAction& Action : Set.Actions)
			{
				if (Action.Name.IsEmpty() || Action.Kind != Kind)
				{
					continue;
				}

				const FString Token = TEXT("#") + FSteamInputVdfWriter::GetActionKey(Action.Name);
				if (Kind == ESteamInputActionKind::StickPad)
				{
					Body += Line(4, FSteamVdfWriter::Quote(Action.Name));
					Body += Line(4, TEXT("{"));
					Body += Pair(5, TEXT("title"), Token);
					Body += Pair(5, TEXT("input_mode"), Action.InputMode.IsEmpty() ? FString(TEXT("joystick_move")) : Action.InputMode);
					Body += Line(4, TEXT("}"));
				}
				else
				{
					Body += Pair(4, Action.Name, Token);
				}
			}

			if (!Body.IsEmpty())
			{
				Out += Line(3, FSteamVdfWriter::Quote(Group));
				Out += Line(3, TEXT("{"));
				Out += Body;
				Out += Line(3, TEXT("}"));
			}
		};

		WriteGroup(TEXT("StickPadGyro"), ESteamInputActionKind::StickPad);
		WriteGroup(TEXT("AnalogTrigger"), ESteamInputActionKind::Trigger);
		WriteGroup(TEXT("Button"), ESteamInputActionKind::Button);

		Out += Line(2, TEXT("}"));
		return Out;
	}
}

FString FSteamInputVdfWriter::GetSetKey(const FString& SetName)
{
	return TEXT("Set_") + SetName;
}

FString FSteamInputVdfWriter::GetActionKey(const FString& ActionName)
{
	return TEXT("Action_") + ActionName;
}

FString FSteamInputVdfWriter::Build(const TArray<FSteamInputVdfSet>& Sets)
{
	FString SetsBody;
	FString LayersBody;
	TArray<TPair<FString, FString>> Localization;

	const auto AddText = [&Localization](const FString& Key, const FString& Text)
	{
		for (const TPair<FString, FString>& Existing : Localization)
		{
			if (Existing.Key == Key)
			{
				return;
			}
		}
		Localization.Emplace(Key, Text);
	};

	for (const FSteamInputVdfSet& Set : Sets)
	{
		if (Set.Name.IsEmpty())
		{
			continue;
		}

		(Set.bLayer ? LayersBody : SetsBody) += BuildSet(Set, Set.bLayer);

		AddText(FSteamInputVdfWriter::GetSetKey(Set.Name), Set.Title.IsEmpty() ? Set.Name : Set.Title);
		for (const FSteamInputVdfAction& Action : Set.Actions)
		{
			if (!Action.Name.IsEmpty())
			{
				AddText(FSteamInputVdfWriter::GetActionKey(Action.Name), Action.Title.IsEmpty() ? Action.Name : Action.Title);
			}
		}
	}

	if (SetsBody.IsEmpty() && LayersBody.IsEmpty())
	{
		return FString();
	}

	FString Out = TEXT("\"In Game Actions\"\n{\n");
	if (!SetsBody.IsEmpty())
	{
		Out += Line(1, TEXT("\"actions\""));
		Out += Line(1, TEXT("{"));
		Out += SetsBody;
		Out += Line(1, TEXT("}"));
	}
	if (!LayersBody.IsEmpty())
	{
		Out += Line(1, TEXT("\"action_layers\""));
		Out += Line(1, TEXT("{"));
		Out += LayersBody;
		Out += Line(1, TEXT("}"));
	}

	Out += Line(1, TEXT("\"localization\""));
	Out += Line(1, TEXT("{"));
	Out += Line(2, TEXT("\"english\""));
	Out += Line(2, TEXT("{"));
	for (const TPair<FString, FString>& Entry : Localization)
	{
		Out += Pair(3, Entry.Key, Entry.Value);
	}
	Out += Line(2, TEXT("}"));
	Out += Line(1, TEXT("}"));
	Out += TEXT("}\n");
	return Out;
}
