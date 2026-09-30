// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Developer-only test bench (Slate test panel). Never part of a Shipping build.
public class SandwichSteamTest : ModuleRules
{
    public SandwichSteamTest(ReadOnlyTargetRules Target) : base(Target)
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
                "GameplayTags",
                "SandwichSteam",
                "SandwichSteamStats",
                "SandwichSteamAchievements",
                "SandwichSteamLeaderboards",
                "SandwichSteamFriends",
                "SandwichSteamPresence",
                "SandwichSteamCloud",
                "SandwichSteamDLC",
                "SandwichSteamScreenshots",
                "SandwichSteamSessions",
                "SandwichSteamVoice",
                "SandwichSteamInput"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "InputCore",
                "Slate",
                "SlateCore",
                "UMG"
            }
        );
    }
}
