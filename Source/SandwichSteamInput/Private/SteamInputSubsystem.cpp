// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SteamInputSubsystem.h"
#include "Core/SteamGameplayTags.h"
#include "Core/SteamInputRules.h"
#include "Core/SteamLog.h"
#include "Core/SteamToolSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputMappingContext.h"
#include "Misc/Paths.h"
#include "SteamGlyphCache.h"
#include "SteamInputBackend.h"
#include "SteamInputKeys.h"
#include "SteamInputSettings.h"
#include "UObject/UObjectGlobals.h"

using SandwichSteam::Input::FActionEvent;
using SandwichSteam::Input::FActionSample;
using SandwichSteam::Input::FSlotTable;
using SandwichSteam::Input::FStateTracker;

namespace
{
	FSteamResult NoController()
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "InputNoController", "No controller is connected. Steam Input only reports controllers of an action file: see Documents/Systems/SteamInput.md."));
	}
}

USteamInputSubsystem* USteamInputSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = (GEngine && WorldContext) ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USteamInputSubsystem>() : nullptr;
}

USteamInputSubsystem::USteamInputSubsystem() = default;
USteamInputSubsystem::~USteamInputSubsystem() = default;

FGameplayTag USteamInputSubsystem::GetFeatureTag() const
{
	return SteamGameplayTags::Feature_Input;
}

bool USteamInputSubsystem::InitializeFeature()
{
#if SANDWICHSTEAM_WITH_STEAMWORKS
	if (!FSlateApplication::IsInitialized())
	{
		return false; // No window, no input (commandlets, servers).
	}

	const USteamInputSettings* Settings = USteamInputSettings::Get();

	FString Manifest;
	if (Settings && !Settings->ActionManifest.FilePath.IsEmpty())
	{
		Manifest = FPaths::IsRelative(Settings->ActionManifest.FilePath)
			? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Settings->ActionManifest.FilePath)
			: Settings->ActionManifest.FilePath;
		FPaths::NormalizeFilename(Manifest);
	}

	Backend = MakeShared<FSteamInputBackend>(GetGameInstance());
	if (!Backend->Init(Manifest))
	{
		Backend.Reset();
		return false;
	}

	const USteamToolSettings* ToolSettings = USteamToolSettings::Get();
	Definition = ToolSettings ? ToolSettings->LoadAppDefinition() : nullptr;
	if (Definition)
	{
		SandwichSteam::Input::RegisterKeys(*Definition);
	}

	Slots = MakeUnique<FSlotTable>();
	Tracker = MakeUnique<FStateTracker>();
	Glyphs = MakeShared<FSteamGlyphCache>(Settings ? Settings->GlyphCacheSize : 32);

	FramesRun = 0;
	EventsEmitted = 0;
	LastActiveSlot = 0;
	bWarnedUnresolved = false;
	ActiveSet = FGameplayTag();
	ActiveLayers.Reset();
	Known.Reset();
	VibrationEnd.Reset();

	ResolveHandles();

	// The starting set: the one of the settings, else the first set that is not a layer.
	if (Definition)
	{
		FGameplayTag Start = Settings ? Settings->DefaultActionSet : FGameplayTag();
		if (!Start.IsValid() || !Definition->FindInputSet(Start) || Definition->FindInputSet(Start)->bLayer)
		{
			Start = FGameplayTag();
			for (const FSteamInputActionSetDef& Set : Definition->InputSets)
			{
				if (!Set.bLayer && Set.Tag.IsValid())
				{
					Start = Set.Tag;
					break;
				}
			}
		}
		ActiveSet = Start;
		RebuildActiveActions();
	}

	bContextsDirty = true;
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USteamInputSubsystem::HandlePostLoadMap);
	SetTickRate(false);

	if (!Definition || Definition->InputSets.IsEmpty())
	{
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: the App Definition has no Input Sets, so no SteamInput_ keys exist. Controllers, glyph and rumble calls still work."));
	}
	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: ready (%d action set(s), starting set %s)."), Definition ? Definition->InputSets.Num() : 0, ActiveSet.IsValid() ? *ActiveSet.ToString() : TEXT("none"));
	return true;
#else
	return false;
#endif
}

void USteamInputSubsystem::ShutdownFeature()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	bFastTick = false;

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	// Nothing may stay pressed or rumbling.
	if (Tracker.IsValid())
	{
		TArray<FActionEvent> Released;
		Tracker->ReleaseAll(Released);
		EmitEvents(Released);
	}

	if (Backend.IsValid() && Slots.IsValid())
	{
		for (int32 Slot = 0; Slot < Slots->NumSlots(); ++Slot)
		{
			if (const uint64 Handle = Slots->GetHandle(Slot))
			{
				Backend->SetVibration(Handle, 0.0f, 0.0f, 0.0f, 0.0f);
			}
		}
	}

	RemoveAllContexts();

	if (Backend.IsValid())
	{
		Backend->Shutdown();
	}

	for (const TPair<int32, FSteamInputController>& Pair : Known)
	{
		OnControllerChanged.Broadcast(Pair.Value, false);
	}

	Known.Reset();
	VibrationEnd.Reset();
	ActiveActions.Reset();
	ActiveLayers.Reset();
	ActiveSet = FGameplayTag();
	SetHandles.Reset();
	ResolvedActions.Reset();
	UnresolvedNames = 0;
	AddedContexts.Reset();
	Definition = nullptr;
	Glyphs.Reset();
	Tracker.Reset();
	Slots.Reset();
	Backend.Reset();
}

// ---- Tick ----

bool USteamInputSubsystem::SetTickRate(bool bFast)
{
	if (TickHandle.IsValid() && bFast == bFastTick)
	{
		return false;
	}

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	const USteamInputSettings* Settings = USteamInputSettings::Get();
	const float Idle = Settings ? FMath::Clamp(Settings->IdlePollSeconds, 0.25f, 5.0f) : 1.0f;
	bFastTick = bFast;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USteamInputSubsystem::TickInput), bFast ? 0.0f : Idle);
	return true;
}

bool USteamInputSubsystem::TickInput(float /*DeltaTime*/)
{
	if (!Backend.IsValid() || !Backend->IsInitialized() || !Slots.IsValid() || !Tracker.IsValid())
	{
		TickHandle.Reset();
		return false; // Removes the ticker.
	}

	++FramesRun;
	Backend->RunFrame();
	Backend->GetConnectedControllers(ConnectedBuffer);

	const USteamInputSettings* Settings = USteamInputSettings::Get();
	TArray<int32> Added;
	TArray<int32> Removed;
	Slots->Update(ConnectedBuffer, Settings ? Settings->MaxControllers : 4, Added, Removed);
	if (!Added.IsEmpty() || !Removed.IsEmpty())
	{
		HandleControllersChanged(Added, Removed);
	}

	const double Now = FPlatformTime::Seconds();

	if (Slots->NumConnected() > 0)
	{
		EventBuffer.Reset();
		for (int32 Slot = 0; Slot < Slots->NumSlots(); ++Slot)
		{
			const uint64 Handle = Slots->GetHandle(Slot);
			if (Handle == 0)
			{
				continue;
			}

			SampleBuffer.Reset();
			for (const FActiveAction& Action : ActiveActions)
			{
				FActionSample Sample;
				Sample.Action = Action.Name;
				Sample.Kind = Action.Kind;
				if (Action.Kind == ESteamInputActionKind::Button)
				{
					Sample.bActive = Backend->ReadDigital(Handle, Action.Handle, Sample.bDown);
				}
				else
				{
					float X = 0.0f;
					float Y = 0.0f;
					Sample.bActive = Backend->ReadAnalog(Handle, Action.Handle, X, Y);
					Sample.Value = FVector2f(X, Y);
				}
				SampleBuffer.Add(Sample);
			}

			Tracker->Update(Slot, SampleBuffer, EventBuffer);
		}
		EmitEvents(EventBuffer);

		// The controller type can change after the connection (Steam finishes identifying it): refresh once a second.
		if (Now - LastTypeCheck >= 1.0)
		{
			LastTypeCheck = Now;
			bool bChanged = false;
			for (TPair<int32, FSteamInputController>& Pair : Known)
			{
				const ESteamInputControllerType Type = Backend->GetControllerType(static_cast<uint64>(Pair.Value.Handle));
				if (Type != Pair.Value.Type)
				{
					Pair.Value.Type = Type;
					bChanged = true;
				}
			}
			if (bChanged)
			{
				OnGlyphsChanged.Broadcast();
			}
		}
	}

	// Timed vibrations.
	for (auto It = VibrationEnd.CreateIterator(); It; ++It)
	{
		if (Now >= It.Value())
		{
			Backend->SetVibration(Slots->GetHandle(It.Key()), 0.0f, 0.0f, 0.0f, 0.0f);
			It.RemoveCurrent();
		}
	}

	if (bContextsDirty)
	{
		SyncContexts();
	}

	// A changed rate replaced this ticker with a new one: this one ends.
	return !SetTickRate(Slots->NumConnected() > 0);
}

void USteamInputSubsystem::EmitEvents(const TArray<FActionEvent>& Events)
{
	for (const FActionEvent& Event : Events)
	{
		++EventsEmitted;
		if (Event.bDown)
		{
			LastActiveSlot = Event.Slot;
		}
		UE_LOG(LogSandwichSteam, VeryVerbose, TEXT("Steam Input: %s slot %d %s (%.2f, %.2f)."), *Event.Action.ToString(), Event.Slot, Event.bDown ? TEXT("down") : TEXT("up"), Event.Value.X, Event.Value.Y);
		SandwichSteam::Input::EmitEvent(Event);
	}
}

void USteamInputSubsystem::HandleControllersChanged(const TArray<int32>& Added, const TArray<int32>& Removed)
{
	TArray<FActionEvent> Released;

	for (const int32 Slot : Removed)
	{
		Tracker->ReleaseSlot(Slot, Released);
		VibrationEnd.Remove(Slot);

		FSteamInputController Info;
		if (const FSteamInputController* Found = Known.Find(Slot))
		{
			Info = *Found;
		}
		Known.Remove(Slot);
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: controller in slot %d disconnected."), Slot);
		EmitEvents(Released);
		Released.Reset();
		OnControllerChanged.Broadcast(Info, false);
	}

	if (!Added.IsEmpty() && UnresolvedNames > 0)
	{
		ResolveHandles(); // The action file may have been loaded after the first look.
	}

	for (const int32 Slot : Added)
	{
		FSteamInputController Info;
		Info.Slot = Slot;
		Info.Handle = static_cast<int64>(Slots->GetHandle(Slot));
		Info.Type = Backend->GetControllerType(static_cast<uint64>(Info.Handle));
		Known.Add(Slot, Info);

		ApplySetsToController(static_cast<uint64>(Info.Handle));
		UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: controller connected in slot %d (%s)."), Slot, *StaticEnum<ESteamInputControllerType>()->GetNameStringByValue(static_cast<int64>(Info.Type)));
		OnControllerChanged.Broadcast(Info, true);
	}

	OnGlyphsChanged.Broadcast();
}

// ---- Names and handles ----

void USteamInputSubsystem::ResolveHandles()
{
	SetHandles.Reset();
	UnresolvedNames = 0;
	TArray<FString> Missing;

	if (Definition && Backend.IsValid())
	{
		for (const FSteamInputActionSetDef& Set : Definition->InputSets)
		{
			if (Set.Tag.IsValid() && !Set.SteamSetName.IsNone())
			{
				const uint64 Handle = Backend->GetActionSetHandle(Set.SteamSetName.ToString());
				SetHandles.Add(Set.Tag, Handle);
				if (Handle == 0)
				{
					++UnresolvedNames;
					Missing.Add(FString::Printf(TEXT("set %s"), *Set.SteamSetName.ToString()));
				}
			}

			for (const FSteamInputActionDef& Action : Set.Actions)
			{
				if (Action.SteamName.IsNone())
				{
					continue;
				}

				FResolvedAction& Resolved = ResolvedActions.FindOrAdd(Action.SteamName);
				Resolved.Kind = Action.Kind;
				if (Resolved.Handle == 0)
				{
					const FString Name = Action.SteamName.ToString();
					Resolved.Handle = Action.Kind == ESteamInputActionKind::Button ? Backend->GetDigitalActionHandle(Name) : Backend->GetAnalogActionHandle(Name);
				}
				if (Resolved.Handle == 0)
				{
					++UnresolvedNames;
					Missing.AddUnique(FString::Printf(TEXT("action %s"), *Action.SteamName.ToString()));
				}
			}
		}
	}

	if (UnresolvedNames > 0 && !bWarnedUnresolved)
	{
		bWarnedUnresolved = true;
		UE_LOG(LogSandwichSteam, Warning, TEXT("Steam Input: Steam does not know %d name(s) of the App Definition (%s). The action file of the app (partner site, or the Action Manifest setting) must contain the same set and action names. Retried when a controller connects."),
			UnresolvedNames, *FString::Join(Missing, TEXT(", ")));
	}

	RebuildActiveActions();
}

void USteamInputSubsystem::RebuildActiveActions()
{
	ActiveActions.Reset();
	if (!Definition)
	{
		return;
	}

	TSet<FName> Seen;
	auto AddSet = [this, &Seen](const FGameplayTag& Tag)
	{
		const FSteamInputActionSetDef* Row = Tag.IsValid() ? Definition->FindInputSet(Tag) : nullptr;
		if (!Row)
		{
			return;
		}

		for (const FSteamInputActionDef& Action : Row->Actions)
		{
			const FResolvedAction* Resolved = ResolvedActions.Find(Action.SteamName);
			if (!Resolved || Resolved->Handle == 0 || Seen.Contains(Action.SteamName))
			{
				continue;
			}
			Seen.Add(Action.SteamName);

			FActiveAction& Active = ActiveActions.AddDefaulted_GetRef();
			Active.Name = Action.SteamName;
			Active.Kind = Resolved->Kind;
			Active.Handle = Resolved->Handle;
		}
	};

	AddSet(ActiveSet);
	for (const FGameplayTag& Layer : ActiveLayers)
	{
		AddSet(Layer);
	}
}

uint64 USteamInputSubsystem::FindSetHandle(const FGameplayTag& Tag) const
{
	const uint64* Handle = SetHandles.Find(Tag);
	return Handle ? *Handle : 0;
}

void USteamInputSubsystem::ApplySetsToController(uint64 Handle) const
{
	if (!Backend.IsValid() || Handle == 0)
	{
		return;
	}

	if (ActiveSet.IsValid())
	{
		Backend->ActivateActionSet(Handle, FindSetHandle(ActiveSet));
	}
	for (const FGameplayTag& Layer : ActiveLayers)
	{
		Backend->ActivateLayer(Handle, FindSetHandle(Layer));
	}
}

// ---- Action sets ----

FSteamResult USteamInputSubsystem::SetActionSet(FGameplayTag SetTag)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const FSteamInputActionSetDef* Row = Definition ? Definition->FindInputSet(SetTag) : nullptr;
	if (!Row)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputNoSet", "No Steam Input set has that gameplay tag. Add it to Input Sets in the Steam App Definition."));
	}
	if (Row->bLayer)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputIsLayer", "That entry is a layer. Use Activate Steam Input Layer."));
	}
	if (SetTag == ActiveSet)
	{
		return FSteamResult::Success();
	}

	const FGameplayTag Old = ActiveSet;
	ActiveSet = SetTag;
	RebuildActiveActions();

	for (int32 Slot = 0; Slot < Slots->NumSlots(); ++Slot)
	{
		ApplySetsToController(Slots->GetHandle(Slot));
	}
	SyncContexts();

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: action set %s."), *SetTag.ToString());
	if (Old.IsValid())
	{
		OnActionSetChanged.Broadcast(Old, false, false);
	}
	OnActionSetChanged.Broadcast(SetTag, false, true);
	OnGlyphsChanged.Broadcast();
	return FSteamResult::Success();
}

FSteamResult USteamInputSubsystem::ActivateLayer(FGameplayTag LayerTag)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	const FSteamInputActionSetDef* Row = Definition ? Definition->FindInputSet(LayerTag) : nullptr;
	if (!Row || !Row->bLayer)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputNoLayer", "No Steam Input layer has that gameplay tag (a set is switched with Set Steam Input Action Set)."));
	}
	if (ActiveLayers.Contains(LayerTag))
	{
		return FSteamResult::Success();
	}

	ActiveLayers.Add(LayerTag);
	RebuildActiveActions();

	const uint64 LayerHandle = FindSetHandle(LayerTag);
	for (int32 Slot = 0; Slot < Slots->NumSlots(); ++Slot)
	{
		Backend->ActivateLayer(Slots->GetHandle(Slot), LayerHandle);
	}
	SyncContexts();

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: layer %s on."), *LayerTag.ToString());
	OnActionSetChanged.Broadcast(LayerTag, true, true);
	OnGlyphsChanged.Broadcast();
	return FSteamResult::Success();
}

FSteamResult USteamInputSubsystem::DeactivateLayer(FGameplayTag LayerTag)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	if (ActiveLayers.Remove(LayerTag) == 0)
	{
		return FSteamResult::Success(); // Not active: nothing to do.
	}
	RebuildActiveActions();

	const uint64 LayerHandle = FindSetHandle(LayerTag);
	for (int32 Slot = 0; Slot < Slots->NumSlots(); ++Slot)
	{
		Backend->DeactivateLayer(Slots->GetHandle(Slot), LayerHandle);
	}
	SyncContexts();

	UE_LOG(LogSandwichSteam, Log, TEXT("Steam Input: layer %s off."), *LayerTag.ToString());
	OnActionSetChanged.Broadcast(LayerTag, true, false);
	OnGlyphsChanged.Broadcast();
	return FSteamResult::Success();
}

// ---- By Steam set name (the rows are keyed by tag internally, so the name resolves to its row's tag) ----

FSteamResult USteamInputSubsystem::ResolveSetName(FName SteamSetName, FGameplayTag& OutTag) const
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result; // Inactive: report that, not a missing row.
	}

	const FSteamInputActionSetDef* Row = Definition ? Definition->FindInputSetByName(SteamSetName) : nullptr;
	if (!Row)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "InputNoSetName", "No Steam Input set or layer is named '{0}'. Add it to Input Sets in the Steam App Definition (the keys and mapping contexts come from there)."), FText::FromName(SteamSetName)));
	}
	if (!Row->Tag.IsValid())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument,
			FText::Format(NSLOCTEXT("SandwichSteam", "InputSetNoTag", "The Steam Input set '{0}' has no gameplay tag in the Steam App Definition. Give it one."), FText::FromName(SteamSetName)));
	}

	OutTag = Row->Tag;
	return FSteamResult::Success();
}

FSteamResult USteamInputSubsystem::SetActionSetByName(FName SteamSetName)
{
	FGameplayTag Tag;
	const FSteamResult Resolved = ResolveSetName(SteamSetName, Tag);
	return Resolved.IsSuccess() ? SetActionSet(Tag) : Resolved;
}

FSteamResult USteamInputSubsystem::ActivateLayerByName(FName SteamSetName)
{
	FGameplayTag Tag;
	const FSteamResult Resolved = ResolveSetName(SteamSetName, Tag);
	return Resolved.IsSuccess() ? ActivateLayer(Tag) : Resolved;
}

FSteamResult USteamInputSubsystem::DeactivateLayerByName(FName SteamSetName)
{
	FGameplayTag Tag;
	const FSteamResult Resolved = ResolveSetName(SteamSetName, Tag);
	return Resolved.IsSuccess() ? DeactivateLayer(Tag) : Resolved;
}

FName USteamInputSubsystem::GetActionSetName() const
{
	const FSteamInputActionSetDef* Row = Definition ? Definition->FindInputSet(ActiveSet) : nullptr;
	return Row ? Row->SteamSetName : NAME_None;
}

// ---- Enhanced Input contexts ----

void USteamInputSubsystem::HandlePostLoadMap(UWorld* /*World*/)
{
	bContextsDirty = true; // Checked by the next tick: the local player's contexts must still match.
}

void USteamInputSubsystem::SyncContexts()
{
	const USteamInputSettings* Settings = USteamInputSettings::Get();
	if (!Definition || (Settings && !Settings->bAutoActivateContexts))
	{
		RemoveAllContexts();
		bContextsDirty = false;
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = (LocalPlayer && World && LocalPlayer->GetPlayerController(World)) ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	if (!InputSubsystem)
	{
		bContextsDirty = true; // No local player controller yet: the ticker tries again.
		return;
	}

	TMap<FGameplayTag, const FSteamInputActionSetDef*> Wanted;
	if (const FSteamInputActionSetDef* Row = ActiveSet.IsValid() ? Definition->FindInputSet(ActiveSet) : nullptr)
	{
		Wanted.Add(ActiveSet, Row);
	}
	for (const FGameplayTag& Layer : ActiveLayers)
	{
		if (const FSteamInputActionSetDef* Row = Definition->FindInputSet(Layer))
		{
			Wanted.Add(Layer, Row);
		}
	}

	// What is not wanted any more (or was added to another local player) goes first, so two sets that share a context swap cleanly.
	for (auto It = AddedContexts.CreateIterator(); It; ++It)
	{
		if (Wanted.Contains(It.Key()) && It.Value().Player.Get() == LocalPlayer)
		{
			continue;
		}

		const UInputMappingContext* Context = It.Value().Context.Get();
		ULocalPlayer* Player = It.Value().Player.Get();
		if (Context && Player)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player))
			{
				Subsystem->RemoveMappingContext(Context);
			}
		}
		It.RemoveCurrent();
	}

	for (const TPair<FGameplayTag, const FSteamInputActionSetDef*>& Pair : Wanted)
	{
		const FAddedContext* Existing = AddedContexts.Find(Pair.Key);
		if (Existing && Existing->Context.IsValid() && InputSubsystem->HasMappingContext(Existing->Context.Get()))
		{
			continue;
		}

		const UInputMappingContext* Context = Pair.Value->InputContext.IsNull() ? nullptr : Cast<UInputMappingContext>(Pair.Value->InputContext.LoadSynchronous());
		if (!Context)
		{
			continue;
		}

		// A context the game added itself stays the game's.
		if (InputSubsystem->HasMappingContext(Context))
		{
			continue;
		}

		InputSubsystem->AddMappingContext(Context, Pair.Value->Priority);
		FAddedContext& Added = AddedContexts.FindOrAdd(Pair.Key);
		Added.Context = Context;
		Added.Player = LocalPlayer;
	}

	bContextsDirty = false;
}

void USteamInputSubsystem::RemoveAllContexts()
{
	for (const TPair<FGameplayTag, FAddedContext>& Pair : AddedContexts)
	{
		const UInputMappingContext* Context = Pair.Value.Context.Get();
		ULocalPlayer* Player = Pair.Value.Player.Get();
		if (Context && Player)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player))
			{
				Subsystem->RemoveMappingContext(Context);
			}
		}
	}
	AddedContexts.Reset();
}

// ---- Controllers ----

TArray<FSteamInputController> USteamInputSubsystem::GetControllers() const
{
	TArray<FSteamInputController> Controllers;
	if (Slots.IsValid())
	{
		for (int32 Slot = 0; Slot < Slots->NumSlots(); ++Slot)
		{
			if (const FSteamInputController* Info = Known.Find(Slot))
			{
				Controllers.Add(*Info);
			}
		}
	}
	return Controllers;
}

bool USteamInputSubsystem::HasController() const
{
	return Slots.IsValid() && Slots->NumConnected() > 0;
}

FSteamResult USteamInputSubsystem::ResolveSlot(int32 Slot, uint64& OutHandle, int32& OutSlot) const
{
	OutHandle = 0;
	OutSlot = INDEX_NONE;
	if (!Slots.IsValid() || Slots->NumConnected() == 0)
	{
		return NoController();
	}

	if (Slot != INDEX_NONE)
	{
		OutHandle = Slots->GetHandle(Slot);
		if (OutHandle == 0)
		{
			return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputBadSlot", "No controller is in that slot."));
		}
		OutSlot = Slot;
		return FSteamResult::Success();
	}

	// The controller used last, else the first one.
	OutHandle = Slots->GetHandle(LastActiveSlot);
	OutSlot = LastActiveSlot;
	for (int32 Index = 0; OutHandle == 0 && Index < Slots->NumSlots(); ++Index)
	{
		OutHandle = Slots->GetHandle(Index);
		OutSlot = Index;
	}
	return OutHandle != 0 ? FSteamResult::Success() : NoController();
}

// ---- Glyphs ----

FSteamResult USteamInputSubsystem::MakeGlyph(FName Action, int32 Slot, FSteamInputGlyph& OutGlyph)
{
	OutGlyph = FSteamInputGlyph();

	const FResolvedAction* Resolved = ResolvedActions.Find(Action);
	if (!Resolved || Resolved->Handle == 0)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputGlyphUnknown", "Steam does not know that action. Check the name in the App Definition and the action file."));
	}

	uint64 Controller = 0;
	int32 UsedSlot = INDEX_NONE;
	FSteamResult Result = ResolveSlot(Slot, Controller, UsedSlot);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	// The top layer answers first, then the set.
	int32 Origin = 0;
	TArray<FGameplayTag, TInlineAllocator<4>> Order;
	for (int32 Index = ActiveLayers.Num() - 1; Index >= 0; --Index)
	{
		Order.Add(ActiveLayers[Index]);
	}
	Order.Add(ActiveSet);

	for (const FGameplayTag& Tag : Order)
	{
		const uint64 SetHandle = FindSetHandle(Tag);
		Origin = Resolved->Kind == ESteamInputActionKind::Button
			? Backend->GetDigitalOrigin(Controller, SetHandle, Resolved->Handle)
			: Backend->GetAnalogOrigin(Controller, SetHandle, Resolved->Handle);
		if (Origin != 0)
		{
			break;
		}
	}

	if (Origin == 0)
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "InputGlyphUnbound", "That action is not bound to a button on this controller in the active action set."));
	}

	const USteamInputSettings* Settings = USteamInputSettings::Get();
	const ESteamGlyphSize Size = Settings ? Settings->GlyphSize : ESteamGlyphSize::Medium;
	const FSteamGlyphKey Key(Origin, Size);

	UTexture2D* Texture = Glyphs->Find(Key);
	if (!Texture)
	{
		Texture = Glyphs->Load(Key, Backend->GetGlyphPath(Origin, Size));
	}

	OutGlyph.Texture = Texture;
	OutGlyph.Label = FText::FromString(Backend->GetOriginName(Origin));
	if (!Texture && OutGlyph.Label.IsEmpty())
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_Failed, NSLOCTEXT("SandwichSteam", "InputGlyphMissing", "Steam has no glyph for that button."));
	}
	return FSteamResult::Success();
}

FSteamResult USteamInputSubsystem::GetGlyphForAction(FName Action, FSteamInputGlyph& OutGlyph, int32 Slot)
{
	OutGlyph = FSteamInputGlyph();

	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}
	return MakeGlyph(Action, Slot, OutGlyph);
}

FSteamResult USteamInputSubsystem::GetGlyphForKey(FKey Key, FSteamInputGlyph& OutGlyph, int32 Slot)
{
	OutGlyph = FSteamInputGlyph();

	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	FString Name;
	if (!SandwichSteam::Input::StripKeyPrefix(Key.GetFName(), Name))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputNotSteamKey", "That is not a Steam Input key (SteamInput_<Action>)."));
	}

	// A stick's axes are SteamInput_<Action>_X / _Y.
	if (!ResolvedActions.Contains(FName(*Name)) && (Name.EndsWith(TEXT("_X")) || Name.EndsWith(TEXT("_Y"))))
	{
		Name.LeftChopInline(2);
	}
	return MakeGlyph(FName(*Name), Slot, OutGlyph);
}

// ---- Feedback ----

FSteamResult USteamInputSubsystem::TriggerVibration(int32 Slot, float Left, float Right, float LeftTrigger, float RightTrigger, float DurationSeconds)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	TArray<int32> Targets;
	if (Slot == INDEX_NONE)
	{
		for (int32 Index = 0; Index < Slots->NumSlots(); ++Index)
		{
			if (Slots->GetHandle(Index) != 0)
			{
				Targets.Add(Index);
			}
		}
	}
	else if (Slots->GetHandle(Slot) != 0)
	{
		Targets.Add(Slot);
	}

	if (Targets.IsEmpty())
	{
		return Slot == INDEX_NONE ? NoController() : FSteamResult::Failure(SteamGameplayTags::Error_InvalidArgument, NSLOCTEXT("SandwichSteam", "InputBadSlotRumble", "No controller is in that slot."));
	}

	const double End = FPlatformTime::Seconds() + DurationSeconds;
	for (const int32 Target : Targets)
	{
		Backend->SetVibration(Slots->GetHandle(Target), Left, Right, LeftTrigger, RightTrigger);
		if (DurationSeconds > 0.0f)
		{
			VibrationEnd.Add(Target, End);
		}
		else
		{
			VibrationEnd.Remove(Target);
		}
	}
	return FSteamResult::Success();
}

FSteamResult USteamInputSubsystem::StopVibration(int32 Slot)
{
	return TriggerVibration(Slot, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

FSteamResult USteamInputSubsystem::SetLedColor(int32 Slot, FColor Color)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	bool bAny = false;
	for (int32 Index = 0; Index < Slots->NumSlots(); ++Index)
	{
		if ((Slot == INDEX_NONE || Slot == Index) && Slots->GetHandle(Index) != 0)
		{
			Backend->SetLedColor(Slots->GetHandle(Index), Color);
			bAny = true;
		}
	}
	return bAny ? FSteamResult::Success() : NoController();
}

FSteamResult USteamInputSubsystem::ResetLedColor(int32 Slot)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	bool bAny = false;
	for (int32 Index = 0; Index < Slots->NumSlots(); ++Index)
	{
		if ((Slot == INDEX_NONE || Slot == Index) && Slots->GetHandle(Index) != 0)
		{
			Backend->ResetLedColor(Slots->GetHandle(Index));
			bAny = true;
		}
	}
	return bAny ? FSteamResult::Success() : NoController();
}

FSteamResult USteamInputSubsystem::ShowBindingPanel(int32 Slot)
{
	FSteamResult Result;
	if (!RequireActive(Result))
	{
		return Result;
	}

	uint64 Controller = 0;
	int32 UsedSlot = INDEX_NONE;
	Result = ResolveSlot(Slot, Controller, UsedSlot);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	if (!Backend->ShowBindingPanel(Controller))
	{
		return FSteamResult::Failure(SteamGameplayTags::Error_NotSupported, NSLOCTEXT("SandwichSteam", "InputNoPanel", "Steam could not open the binding panel (needs the Steam overlay and a controller Steam can configure)."));
	}
	return FSteamResult::Success();
}

#if SANDWICHSTEAM_WITH_DEBUG
FString USteamInputSubsystem::BuildDebugString() const
{
	FString Report = FString::Printf(TEXT("Steam.Input: feature %s\n"), IsFeatureActive() ? TEXT("active") : TEXT("inactive"));
	if (!IsFeatureActive() || !Slots.IsValid())
	{
		return Report;
	}

	Report += FString::Printf(TEXT("  Controllers: %d connected, ticker %s, last active slot %d\n"), Slots->NumConnected(), bFastTick ? TEXT("every frame") : TEXT("idle poll"), LastActiveSlot);
	for (const int32 Slot : Slots->GetOccupiedSlots())
	{
		if (const FSteamInputController* Info = Known.Find(Slot))
		{
			Report += FString::Printf(TEXT("    slot %d: %s (handle %llu)\n"), Slot, *StaticEnum<ESteamInputControllerType>()->GetNameStringByValue(static_cast<int64>(Info->Type)), static_cast<uint64>(Info->Handle));
		}
	}

	Report += FString::Printf(TEXT("  Action set: %s, layers: %d\n"), ActiveSet.IsValid() ? *ActiveSet.ToString() : TEXT("none"), ActiveLayers.Num());
	for (const FGameplayTag& Layer : ActiveLayers)
	{
		Report += FString::Printf(TEXT("    layer: %s\n"), *Layer.ToString());
	}

	Report += FString::Printf(TEXT("  Definition: %s, %d set(s), %d action name(s), %d not known to Steam\n"),
		Definition ? *Definition->GetName() : TEXT("none"), Definition ? Definition->InputSets.Num() : 0, ResolvedActions.Num(), UnresolvedNames);
	Report += FString::Printf(TEXT("  Reading %d action(s) per controller and frame\n"), ActiveActions.Num());
	for (const FActiveAction& Action : ActiveActions)
	{
		Report += FString::Printf(TEXT("    %s (%s)\n"), *Action.Name.ToString(), *StaticEnum<ESteamInputActionKind>()->GetNameStringByValue(static_cast<int64>(Action.Kind)));
	}

	const USteamInputSettings* Settings = USteamInputSettings::Get();
	Report += FString::Printf(TEXT("  Mapping contexts: %s, %d added by the feature%s\n"), Settings && Settings->bAutoActivateContexts ? TEXT("automatic") : TEXT("manual"), AddedContexts.Num(), bContextsDirty ? TEXT(" (waiting for the local player)") : TEXT(""));
	Report += FString::Printf(TEXT("  Glyph cache: %d of %d\n"), Glyphs.IsValid() ? Glyphs->Num() : 0, Glyphs.IsValid() ? Glyphs->GetCapacity() : 0);
	Report += FString::Printf(TEXT("  Counters: %lld frame(s) run, %lld event(s) sent, %d action(s) held\n"), FramesRun, EventsEmitted, Tracker.IsValid() ? Tracker->NumHeld() : 0);
	return Report;
}
#endif
