// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class TapRep : ModuleRules
{
    public TapRep(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Platform == UnrealTargetPlatform.IOS ||
            Target.Platform == UnrealTargetPlatform.Android)
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
            "Json", "JsonUtilities"
        });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.Add("Launch");

            AdditionalPropertiesForReceipt.Add(
                "AndroidPlugin",
                Path.Combine(ModuleDirectory, "TapRep_Android_UPL.xml"));

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
        }

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.Add("Launch");

            PublicFrameworks.AddRange(new string[]
            {
                "UIKit", "Foundation", "SystemConfiguration", "WebKit"
            });

            AdditionalPropertiesForReceipt.Add(
                "IOSPlugin",
                Path.Combine(ModuleDirectory, "TapRep_iOS_UPL.xml"));

            string IosThirdPartyRoot = Path.GetFullPath(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string RepSdkPath = Path.Combine(IosThirdPartyRoot, "TapTapRepSDK.xcframework");
            bool bSdkAvailable = Directory.Exists(RepSdkPath);
            PublicDefinitions.Add("TAP_REP_IOS_SDK_AVAILABLE=" + (bSdkAvailable ? "1" : "0"));

            if (bSdkAvailable)
            {
                PublicAdditionalFrameworks.Add(new Framework("TapTapRepSDK", RepSdkPath, "", true));
                PublicIncludePaths.Add(Path.Combine(
                    RepSdkPath, "ios-arm64", "TapTapRepSDK.framework", "Headers"));
            }

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/iOS"));
        }
    }
}
