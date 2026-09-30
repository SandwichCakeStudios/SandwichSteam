// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SandwichSteam : ModuleRules
{
    public SandwichSteam(ReadOnlyTargetRules Target) : base(Target)
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
                "OnlineSubsystem"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "OnlineSubsystemUtils",
                "Projects",
                "Slate",
                "SlateCore"
            }
        );

        // Steam is initialized by the engine's OnlineSubsystemSteam (the Steam host).
        // Features call raw Steamworks through the engine's Steamworks module; we never
        // bundle our own steam_api library and never call SteamAPI_Init.
        bool bWithSteamworks = Target.Platform == UnrealTargetPlatform.Win64
            || Target.Platform == UnrealTargetPlatform.Mac
            || Target.Platform == UnrealTargetPlatform.Linux;

        if (bWithSteamworks)
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
        }

        PublicDefinitions.Add(bWithSteamworks ? "SANDWICHSTEAM_WITH_STEAMWORKS=1" : "SANDWICHSTEAM_WITH_STEAMWORKS=0");

        // ISteamUser::GetAuthTicketForWebApi only exists in newer Steamworks SDKs. Detect it in the SDK the engine ships
        // so the plugin compiles against either. Without it the Web API ticket request fails with Steam.Error.NotSupported.
        bool bWithWebApiTicket = false;
        if (bWithSteamworks)
        {
            try
            {
                string SteamRoot = Path.Combine(Target.UEThirdPartySourceDirectory, "Steamworks");
                if (Directory.Exists(SteamRoot))
                {
                    string[] Headers = Directory.GetFiles(SteamRoot, "isteamuser.h", SearchOption.AllDirectories);
                    if (Headers.Length > 0)
                    {
                        System.Array.Sort(Headers);
                        string Text = File.ReadAllText(Headers[Headers.Length - 1]); // highest SDK version folder
                        bWithWebApiTicket = Text.Contains("GetAuthTicketForWebApi") && Text.Contains("GetTicketForWebApiResponse_t");
                    }
                }
            }
            catch (System.Exception)
            {
                bWithWebApiTicket = false;
            }
        }
        PublicDefinitions.Add(bWithWebApiTicket ? "SANDWICHSTEAM_WITH_WEBAPI_TICKET=1" : "SANDWICHSTEAM_WITH_WEBAPI_TICKET=0");

        // Debug console commands and the dev overlay exist in every configuration except Shipping.
        bool bWithDebug = Target.Configuration != UnrealTargetConfiguration.Shipping;
        PublicDefinitions.Add(bWithDebug ? "SANDWICHSTEAM_WITH_DEBUG=1" : "SANDWICHSTEAM_WITH_DEBUG=0");
    }
}
