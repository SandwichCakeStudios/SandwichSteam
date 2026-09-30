// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Text.RegularExpressions;

// Steam stats (ISteamUserStats). Also owns the ISteamUserStats backend that the Achievements module shares.
public class SandwichSteamStats : ModuleRules
{
    public SandwichSteamStats(ReadOnlyTargetRules Target) : base(Target)
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
                "Json"
            }
        );

        bool bWithSteamworks = Target.Platform == UnrealTargetPlatform.Win64
            || Target.Platform == UnrealTargetPlatform.Mac
            || Target.Platform == UnrealTargetPlatform.Linux;

        if (bWithSteamworks)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }

        // Newer Steamworks SDKs request the stats automatically and no longer declare ISteamUserStats::RequestCurrentStats.
        // Detect it in the SDK the engine ships so the module compiles against either.
        bool bStatsRequest = false;
        if (bWithSteamworks)
        {
            try
            {
                string SteamRoot = Path.Combine(Target.UEThirdPartySourceDirectory, "Steamworks");
                if (Directory.Exists(SteamRoot))
                {
                    string[] Headers = Directory.GetFiles(SteamRoot, "isteamuserstats.h", SearchOption.AllDirectories);
                    if (Headers.Length > 0)
                    {
                        System.Array.Sort(Headers);
                        string Text = File.ReadAllText(Headers[Headers.Length - 1]); // highest SDK version folder
                        bStatsRequest = Regex.IsMatch(Text, @"(?m)^\s*virtual\s+bool\s+RequestCurrentStats\s*\(");
                    }
                }
            }
            catch (System.Exception)
            {
                bStatsRequest = false;
            }
        }
        PublicDefinitions.Add(bStatsRequest ? "SANDWICHSTEAM_WITH_STATS_REQUEST=1" : "SANDWICHSTEAM_WITH_STATS_REQUEST=0");
    }
}
