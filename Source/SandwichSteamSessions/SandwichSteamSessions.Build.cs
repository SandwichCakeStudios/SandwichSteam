// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam sessions, lobbies, invites and travel (OSS IOnlineSession + a little raw ISteamMatchmaking / ISteamFriends).
// Remove this module (and its uplugin entry) to ship without it. It only needs the core.
// The SteamSockets plugin is a runtime/ini requirement (net driver class names), not a link dependency.
public class SandwichSteamSessions : ModuleRules
{
    public SandwichSteamSessions(ReadOnlyTargetRules Target) : base(Target)
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
                "OnlineSubsystemUtils"
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
