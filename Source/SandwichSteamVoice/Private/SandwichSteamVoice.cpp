// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

#include "SandwichSteamVoice.h"
#include "Core/SteamLog.h"
#include "Debug/SteamDebugSection.h"
#include "SteamVoiceSubsystem.h"

#if SANDWICHSTEAM_WITH_DEBUG

namespace
{
	void LogResult(FOutputDevice& Output, const TCHAR* Command, const FSteamResult& Result, const TCHAR* Done)
	{
		Output.Logf(TEXT("%s: %s"), Command, Result.IsSuccess() ? Done : *Result.Message.ToString());
	}

	void DumpVoice(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Output)
	{
		if (const USteamVoiceSubsystem* Voice = FSteamDebugCommandSet::FindFeature<USteamVoiceSubsystem>(World, Output))
		{
			Output.Log(*Voice->BuildDebugString());
		}
	}

	/** Steam.Voice.Enable <0|1> */
	void EnableVoice(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamVoiceSubsystem* Voice = FSteamDebugCommandSet::FindFeature<USteamVoiceSubsystem>(World, Output);
		if (!Voice)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Voice.Enable <0|1>"));
			return;
		}

		const bool bEnable = FCString::Atoi(*Args[0]) != 0;
		LogResult(Output, TEXT("Steam.Voice.Enable"), Voice->SetVoiceEnabled(bEnable), bEnable ? TEXT("voice enabled") : TEXT("voice disabled"));
	}

	/** Steam.Voice.Mode <ptt|open> */
	void SetMode(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamVoiceSubsystem* Voice = FSteamDebugCommandSet::FindFeature<USteamVoiceSubsystem>(World, Output);
		if (!Voice)
		{
			return;
		}

		if (Args.IsEmpty() || !(Args[0].StartsWith(TEXT("p"), ESearchCase::IgnoreCase) || Args[0].StartsWith(TEXT("o"), ESearchCase::IgnoreCase)))
		{
			Output.Log(TEXT("Usage: Steam.Voice.Mode <ptt|open>"));
			return;
		}

		const bool bPushToTalk = Args[0].StartsWith(TEXT("p"), ESearchCase::IgnoreCase);
		LogResult(Output, TEXT("Steam.Voice.Mode"), Voice->SetVoiceMode(bPushToTalk ? ESteamVoiceMode::PushToTalk : ESteamVoiceMode::OpenMic), bPushToTalk ? TEXT("push to talk") : TEXT("open mic"));
	}

	/** Steam.Voice.Talk <0|1>: the same as holding the push to talk key. */
	void Talk(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamVoiceSubsystem* Voice = FSteamDebugCommandSet::FindFeature<USteamVoiceSubsystem>(World, Output);
		if (!Voice)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Log(TEXT("Usage: Steam.Voice.Talk <0|1>"));
			return;
		}

		const bool bTalk = FCString::Atoi(*Args[0]) != 0;
		LogResult(Output, TEXT("Steam.Voice.Talk"), bTalk ? Voice->StartTalking() : Voice->StopTalking(), bTalk ? TEXT("microphone requested") : TEXT("microphone released"));
	}

	/** Steam.Voice.Mute <SteamID64> and Steam.Voice.Unmute <SteamID64> */
	void MuteOrUnmute(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output, bool bMute)
	{
		USteamVoiceSubsystem* Voice = FSteamDebugCommandSet::FindFeature<USteamVoiceSubsystem>(World, Output);
		if (!Voice)
		{
			return;
		}

		FSteamId Player;
		if (Args.IsEmpty() || !FSteamId::FromString(Args[0], Player))
		{
			Output.Logf(TEXT("Usage: Steam.Voice.%s <SteamID64>"), bMute ? TEXT("Mute") : TEXT("Unmute"));
			return;
		}

		LogResult(Output, bMute ? TEXT("Steam.Voice.Mute") : TEXT("Steam.Voice.Unmute"), bMute ? Voice->MutePlayer(Player) : Voice->UnmutePlayer(Player), bMute ? TEXT("muted") : TEXT("unmuted"));
	}

	void MutePlayer(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		MuteOrUnmute(Args, World, Output, true);
	}

	void UnmutePlayer(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		MuteOrUnmute(Args, World, Output, false);
	}

	/** Steam.Voice.Volume <0..2> */
	void SetVolume(const TArray<FString>& Args, UWorld* World, FOutputDevice& Output)
	{
		USteamVoiceSubsystem* Voice = FSteamDebugCommandSet::FindFeature<USteamVoiceSubsystem>(World, Output);
		if (!Voice)
		{
			return;
		}

		if (Args.IsEmpty())
		{
			Output.Logf(TEXT("Steam.Voice.Volume: %.2f. Usage: Steam.Voice.Volume <0..2>"), Voice->GetMasterVolume());
			return;
		}

		LogResult(Output, TEXT("Steam.Voice.Volume"), Voice->SetMasterVolume(FCString::Atof(*Args[0])), TEXT("volume set"));
	}

	const FName DebugSectionId(TEXT("Voice"));

	FString ReportVoice(UWorld* World)
	{
		const USteamVoiceSubsystem* Voice = SandwichSteam::Debug::FindFeatureSubsystem<USteamVoiceSubsystem>(World);
		return Voice ? Voice->BuildDebugString() : SandwichSteam::Debug::GetFeatureMissingText();
	}
}

#endif // SANDWICHSTEAM_WITH_DEBUG

void FSandwichSteamVoiceModule::StartupModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	FSteamDebugSection Section;
	Section.Id = DebugSectionId;
	Section.Title = NSLOCTEXT("SandwichSteamVoice", "DebugTitle", "Voice");
	Section.Order = 100;
	Section.BuildReport = &ReportVoice;
	SandwichSteam::Debug::RegisterSection(MoveTemp(Section));

	Commands.Add(TEXT("Steam.Voice.Dump"),
		TEXT("Prints the voice state: mode, microphone, push to talk binding, who talks, who is muted."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DumpVoice));

	Commands.Add(TEXT("Steam.Voice.Enable"),
		TEXT("Turns voice on or off: Steam.Voice.Enable <0|1>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&EnableVoice));

	Commands.Add(TEXT("Steam.Voice.Mode"),
		TEXT("Switches the voice mode: Steam.Voice.Mode <ptt|open>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetMode));

	Commands.Add(TEXT("Steam.Voice.Talk"),
		TEXT("Opens or closes the microphone like the push to talk key: Steam.Voice.Talk <0|1>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&Talk));

	Commands.Add(TEXT("Steam.Voice.Mute"),
		TEXT("Mutes a player: Steam.Voice.Mute <SteamID64>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&MutePlayer));

	Commands.Add(TEXT("Steam.Voice.Unmute"),
		TEXT("Unmutes a player: Steam.Voice.Unmute <SteamID64>."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&UnmutePlayer));

	Commands.Add(TEXT("Steam.Voice.Volume"),
		TEXT("Sets (or reads) the master volume of the voice sound class: Steam.Voice.Volume [0..2]."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SetVolume));
#endif
}

void FSandwichSteamVoiceModule::ShutdownModule()
{
#if SANDWICHSTEAM_WITH_DEBUG
	SandwichSteam::Debug::UnregisterSection(DebugSectionId);
	Commands.Reset();
#endif
}

IMPLEMENT_MODULE(FSandwichSteamVoiceModule, SandwichSteamVoice)
