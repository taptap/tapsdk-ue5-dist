// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class TapUpdate : ModuleRules
{
    public TapUpdate(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
        }
        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "TapCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json"
        });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.Add("Launch");

            AdditionalPropertiesForReceipt.Add(
                "AndroidPlugin",
                Path.Combine(ModuleDirectory, "TapUpdate_Android_UPL.xml"));

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
        }
    }
}
