// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam in-session voice chat (OSS IOnlineVoice over the game connection, Enhanced Input push to talk, a little raw ISteamFriends
// for the blocked check). Remove this module (and its uplugin entry) to ship without it. It only needs the core.
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
