// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class TapShare : ModuleRules
{
    public TapShare(ReadOnlyTargetRules Target) : base(Target)
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

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.Add("Launch");

            AdditionalPropertiesForReceipt.Add(
                "AndroidPlugin",
                Path.Combine(ModuleDirectory, "TapShare_Android_UPL.xml"));

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
        }

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.Add("Launch");

            PublicFrameworks.AddRange(new string[]
            {
                "UIKit", "Foundation"
            });

            AdditionalPropertiesForReceipt.Add(
                "IOSPlugin",
                Path.Combine(ModuleDirectory, "TapShare_iOS_UPL.xml"));

            string IosThirdPartyRoot = Path.GetFullPath(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string ShareSdkPath = Path.Combine(IosThirdPartyRoot, "TapTapShareSDK.xcframework");
            bool bSdkAvailable = Directory.Exists(ShareSdkPath);
            PublicDefinitions.Add("TAP_SHARE_IOS_SDK_AVAILABLE=" + (bSdkAvailable ? "1" : "0"));

            if (bSdkAvailable)
            {
                PublicAdditionalFrameworks.Add(new Framework("TapTapShareSDK", ShareSdkPath, "", true));
                PublicIncludePaths.Add(Path.Combine(
                    ShareSdkPath, "ios-arm64", "TapTapShareSDK.framework", "Headers"));
            }

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/iOS"));
        }
    }
}
