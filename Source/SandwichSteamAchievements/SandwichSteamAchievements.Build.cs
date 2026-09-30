// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam achievements. Remove this module (and its uplugin entry) to ship without achievements.
// It builds on the Stats module, which owns the shared ISteamUserStats backend. It needs no Steamworks headers itself.
public class SandwichSteamAchievements : ModuleRules
{
    public SandwichSteamAchievements(ReadOnlyTargetRules Target) : base(Target)
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
                "SandwichSteam"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "DeveloperSettings",
                "SandwichSteamStats"
            }
        );
    }
}
