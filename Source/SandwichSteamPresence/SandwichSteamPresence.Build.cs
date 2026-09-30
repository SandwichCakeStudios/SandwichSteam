// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam rich presence (ISteamFriends). Remove this module (and its uplugin entry) to ship without it.
// It only needs the core: it does not depend on the Friends module.
public class SandwichSteamPresence : ModuleRules
{
    public SandwichSteamPresence(ReadOnlyTargetRules Target) : base(Target)
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

        bool bWithSteamworks = Target.Platform == UnrealTargetPlatform.Win64
            || Target.Platform == UnrealTargetPlatform.Mac
            || Target.Platform == UnrealTargetPlatform.Linux;

        if (bWithSteamworks)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }
    }
}
