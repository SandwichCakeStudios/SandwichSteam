// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam in-session voice chat: the Steam voice pipeline (Steam capture and codec, SteamNetworkingMessages, a voice worker thread,
// USteamVoiceComponent speakers) or the OSS IOnlineVoice fallback, Enhanced Input push to talk, mute + Steam block list.
// Remove this module (and its uplugin entry) to ship without it. It only needs the core.
public class SandwichSteamVoice : ModuleRules
{
    public SandwichSteamVoice(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "DeveloperSettings",
                "GameplayTags",
                "OnlineSubsystem",
                "AudioMixer",       // USynthComponent (USteamVoiceComponent derives from it)
                "SignalProcessing", // Audio::FPatchSplitter / FPatchOutput between the voice worker and the speakers
                "SandwichSteam"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "EnhancedInput",
                "InputCore"
            }
        );

        bool bWithSteamworks = Target.Platform == UnrealTargetPlatform.Win64
            || Target.Platform == UnrealTargetPlatform.Mac
            || Target.Platform == UnrealTargetPlatform.Linux;

        if (bWithSteamworks)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }
    }
}
