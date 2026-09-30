// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam Input (raw ISteamInput) bridged to Enhanced Input through registered keys. Remove this module (and its uplugin entry)
// to ship without it. It only needs the core.
public class SandwichSteamInput : ModuleRules
{
    public SandwichSteamInput(ReadOnlyTargetRules Target) : base(Target)
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
                "InputCore",
                "SandwichSteam"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ApplicationCore",
                "EnhancedInput",
                "Slate",
                "SlateCore"
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
