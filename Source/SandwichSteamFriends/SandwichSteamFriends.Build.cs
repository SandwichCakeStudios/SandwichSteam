// Copyright 2026 Sandwich Cake Studios. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Steam friends list (ISteamFriends). Remove this module (and its uplugin entry) to ship without it.
// It only needs the core (identity, overlay dialogs); it does not depend on the Presence module.
public class SandwichSteamFriends : ModuleRules
{
    public SandwichSteamFriends(ReadOnlyTargetRules Target) : base(Target)
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

        // Friend groups (tags) are not in every Steamworks SDK. Detect them in the SDK the engine ships, so the module
        // compiles against either. Without them Get Steam Friend Groups returns an empty list.
        bool bWithFriendGroups = false;
        if (bWithSteamworks)
        {
            try
            {
                string SteamRoot = Path.Combine(Target.UEThirdPartySourceDirectory, "Steamworks");
                if (Directory.Exists(SteamRoot))
                {
                    string[] Headers = Directory.GetFiles(SteamRoot, "isteamfriends.h", SearchOption.AllDirectories);
                    if (Headers.Length > 0)
                    {
                        System.Array.Sort(Headers);
                        string Text = File.ReadAllText(Headers[Headers.Length - 1]); // highest SDK version folder
                        bWithFriendGroups = Text.Contains("GetFriendsGroupCount") && Text.Contains("GetFriendsGroupMembersList");
                    }
                }
            }
            catch (System.Exception)
            {
                bWithFriendGroups = false;
            }
        }
        PrivateDefinitions.Add(bWithFriendGroups ? "SANDWICHSTEAM_WITH_FRIEND_GROUPS=1" : "SANDWICHSTEAM_WITH_FRIEND_GROUPS=0");
    }
}
