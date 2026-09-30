// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;

public class SandwichSteamEditor : ModuleRules
{
    public SandwichSteamEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "SandwichSteam"
            }
        );

        // Later phases add what they first use (HTTP for the optional Web API import, the publish tool's process runner, ...).
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "InputCore",
                "ApplicationCore",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "ToolMenus",
                "Projects",
                "DeveloperSettings",
                "DesktopPlatform",
                "SourceControl",
                "GameplayTags",
                "GameplayTagsEditor",
                "PropertyEditor",
                "AssetTools",
                "AssetDefinition",
                "MessageLog",
                "Settings",
                "Json",
                "HTTP"
            }
        );

        // FSteamLiveCodingGuard: Live Coding must be off while UAT packages. The module only exists on Windows.
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("LiveCoding");
        }
    }
}
