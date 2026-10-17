// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TapLeaderboard : ModuleRules
{
    public TapLeaderboard(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "TapCore", "TapLogin", "TapProfile"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json", "JsonUtilities"
        });

        // ── Android ──────────────────────────────────────────────────────────
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.Add("Launch");

            AdditionalPropertiesForReceipt.Add(
                "AndroidPlugin",
                Path.Combine(ModuleDirectory, "TapLeaderboard_Android_UPL.xml"));

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
        }

        // ── iOS ──────────────────────────────────────────────────────────────
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.Add("Launch");

            PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation" });

            AdditionalPropertiesForReceipt.Add(
                "IOSPlugin",
                Path.Combine(ModuleDirectory, "TapLeaderboard_iOS_UPL.xml"));

            // TapTapLeaderboardSDK.xcframework 由原生团队单独交付；
            // 缺失时停止构建，避免运行时请求没有回调并阻塞后续队列。
            string IosThirdPartyRoot = Path.GetFullPath(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string LeaderboardSdkPath = Path.Combine(IosThirdPartyRoot, "TapTapLeaderboardSDK.xcframework");
            bool bSdkAvailable = Directory.Exists(LeaderboardSdkPath);
            if (!bSdkAvailable)
            {
                throw new BuildException(
                    "TapLeaderboard requires TapTapLeaderboardSDK.xcframework for iOS. " +
                    "Restore the framework from the matching TapLeaderboard plugin package to: " + LeaderboardSdkPath);
            }
            PublicDefinitions.Add("TAP_LEADERBOARD_IOS_SDK_AVAILABLE=" + (bSdkAvailable ? "1" : "0"));
            if (bSdkAvailable)
            {
                // TapTapLeaderboardResource.bundle 承载排行榜 UI 图片资源，必须随 xcframework 一起拷入 app 包，
                // 否则 TapLeaderboardBundleUtils.getImage 在运行时 NSBundle bundleWithURL: 传 nil URL 崩溃。
                string LeaderboardResourcePath = Path.Combine(IosThirdPartyRoot, "TapTapLeaderboardResource.bundle");
                PublicAdditionalFrameworks.Add(new Framework(
                    "TapTapLeaderboardSDK",
                    LeaderboardSdkPath,
                    Directory.Exists(LeaderboardResourcePath) ? LeaderboardResourcePath : null,
                    true));
                PublicIncludePaths.Add(Path.Combine(
                    LeaderboardSdkPath, "ios-arm64", "TapTapLeaderboardSDK.framework", "Headers"));
                // BridgeLeaderboardService.h 与 xcframework 并列放在 Frameworks 目录下
                PublicIncludePaths.Add(IosThirdPartyRoot);

                // TapTapKingfisher + TapTapProfileSDK 由 TapProfile 插件统一提供，此处不重复链接
            }

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
        }
    }
}
