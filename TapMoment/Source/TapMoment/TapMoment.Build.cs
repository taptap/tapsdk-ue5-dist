// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class TapMoment : ModuleRules
{
    public TapMoment(ReadOnlyTargetRules Target) : base(Target)
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
            "TapCore", "TapLogin"
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
                Path.Combine(ModuleDirectory, "TapMoment_Android_UPL.xml"));

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
        }

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.Add("Launch");

            PublicFrameworks.AddRange(new string[]
            {
                "UIKit", "Foundation", "AVFoundation", "CoreTelephony",
                "MobileCoreServices", "Photos", "SystemConfiguration", "WebKit"
            });

            AdditionalPropertiesForReceipt.Add(
                "IOSPlugin",
                Path.Combine(ModuleDirectory, "TapMoment_iOS_UPL.xml"));

            string IosThirdPartyRoot = Path.GetFullPath(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string MomentSdkPath = Path.Combine(IosThirdPartyRoot, "TapTapMomentSDK.xcframework");
            bool bSdkAvailable = Directory.Exists(MomentSdkPath);
            PublicDefinitions.Add("TAP_MOMENT_IOS_SDK_AVAILABLE=" + (bSdkAvailable ? "1" : "0"));

            if (bSdkAvailable)
            {
                // TapTapMomentResource.bundle 承载动态本地化字符串等资源，必须随 xcframework 一起拷入 app 包，
                // 否则 TapMomentLangUtil.getLocalizedString 在运行时找不到 bundle 而崩溃（与 TapLeaderboard 同构问题）。
                string MomentResourcePath = Path.Combine(IosThirdPartyRoot, "TapTapMomentResource.bundle");
                PublicAdditionalFrameworks.Add(new Framework(
                    "TapTapMomentSDK",
                    MomentSdkPath,
                    Directory.Exists(MomentResourcePath) ? MomentResourcePath : null,
                    true));
                PublicIncludePaths.Add(Path.Combine(
                    MomentSdkPath, "ios-arm64", "TapTapMomentSDK.framework", "Headers"));
            }

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/iOS"));
        }
    }
}
