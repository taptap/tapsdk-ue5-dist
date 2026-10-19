// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#endif

public class TapAchievement : ModuleRules
{
    public TapAchievement(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        // 混编 ObjC++(.mm) / JNI 的移动端关闭 PCH，与其它 Tap* 移动端模块一致。
        // Mac Desktop 只编译 Private/Desktop/ 的纯 .cpp，沿用共享 PCH。
        if (Target.Platform == UnrealTargetPlatform.IOS ||
            Target.Platform == UnrealTargetPlatform.Android)
        {
            PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
        }
        bUseUnity = false;

        FileReference fileRef = new FileReference(Path.Combine(PluginDirectory, Name + ".uplugin"));
        PluginInfo plugin = new PluginInfo(fileRef, PluginType.Project);
        PublicDefinitions.Add(Name + "_VERSION=TEXT(\"" + plugin.Descriptor.VersionName + "\")");

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "TapCore", "TapLogin"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UMG", "SlateCore", "Json", "JsonUtilities"
        });

        // ── Android：通用 EngineBridge 桥接（路径 B）──────────────────────────
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // Launch 提供 FAndroidApplication::GetJavaEnv / GetGameActivityThis。
            PrivateDependencyModuleNames.Add("Launch");

            AdditionalPropertiesForReceipt.Add(
                "AndroidPlugin",
                Path.Combine(ModuleDirectory, "TapAchievement_Android_UPL.xml"));

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
        }

        // ── iOS：直接调原生 BridgeAchievementService（ObjC++）────────────────
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            // Launch 为 ObjC++ 与 UE5 混编提供 IOSAppDelegate 等前置声明。
            PrivateDependencyModuleNames.Add("Launch");

            PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation" });

            AdditionalPropertiesForReceipt.Add(
                "IOSPlugin",
                Path.Combine(ModuleDirectory, "TapAchievement_iOS_UPL.xml"));

            // TapTapAchievementSDK.xcframework 由原生团队单独交付。
            // 未提交 xcframework 时，若存在松散头文件 BridgeAchievementService.h（CocoaPods 或手动链接场景），
            // 也启用桥接编译——实现由原生 SDK 在链接期注入。
            // IOSBridge.mm 通过 TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE 宏感知是否可用。
            string IosThirdPartyRoot = Path.GetFullPath(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string AchievementSdkPath = Path.Combine(IosThirdPartyRoot, "TapTapAchievementSDK.xcframework");
            string AchievementHeaderPath = Path.Combine(IosThirdPartyRoot, "BridgeAchievementService.h");
            bool bAchievementSdkAvailable = Directory.Exists(AchievementSdkPath);
            bool bAchievementHeaderAvailable = File.Exists(AchievementHeaderPath);
            bool bAchievementAvailable = bAchievementSdkAvailable || bAchievementHeaderAvailable;
            PublicDefinitions.Add("TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE=" + (bAchievementAvailable ? "1" : "0"));
            if (bAchievementSdkAvailable)
            {
                // TapTapAchievementResource.bundle 承载成就 UI 图片资源，必须随 xcframework 一起拷入 app 包，
                // 否则 TapAchievementUIUtil 在运行时找不到 bundle 而崩溃（与 TapLeaderboard 同构问题）。
                string AchievementResourcePath = Path.Combine(IosThirdPartyRoot, "TapTapAchievementResource.bundle");
                PublicAdditionalFrameworks.Add(new Framework(
                    "TapTapAchievementSDK",
                    AchievementSdkPath,
                    Directory.Exists(AchievementResourcePath) ? AchievementResourcePath : null,
                    true));
                PublicIncludePaths.Add(Path.Combine(
                    AchievementSdkPath, "ios-arm64", "TapTapAchievementSDK.framework", "Headers"));
                // BridgeAchievementService.h 与 xcframework 并列放在 Frameworks 目录下，无条件加入
                PublicIncludePaths.Add(IosThirdPartyRoot);
            }
            else if (bAchievementHeaderAvailable)
            {
                // 松散头文件：仅提供编译期声明，实现由 CocoaPods 或手动链接的原生 SDK 注入
                PublicIncludePaths.Add(IosThirdPartyRoot);
            }

            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
        }

        // ── Desktop：现有 HTTP + WBP toast 实现 ──────────────────────────────
        if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Mac)
        {
            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Desktop"));
            if (Target.Configuration == UnrealTargetConfiguration.Debug ||
                Target.Configuration == UnrealTargetConfiguration.DebugGame ||
                Target.Configuration == UnrealTargetConfiguration.Development)
            {
                PrivateIncludePaths.Add(
                    Path.Combine(ModuleDirectory, "Private/Desktop/Tests"));
            }
        }
    }
}
