// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class TapRelationLite : ModuleRules
{
    public TapRelationLite(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.Android)
            PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "TapCore", "TapLogin", "TapProfile" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities" });

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PrivateDependencyModuleNames.Add("Launch");
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "TapRelationLite_Android_UPL.xml"));
            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
        }

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.Add("Launch");
            PrivateDependencyModuleNames.Add("ApplicationCore"); // FIOSCoreDelegates::OnOpenURL
            PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation" });
            AdditionalPropertiesForReceipt.Add("IOSPlugin", Path.Combine(ModuleDirectory, "TapRelationLite_iOS_UPL.xml"));

            string IosThirdPartyRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string SdkPath = Path.Combine(IosThirdPartyRoot, "TapTapRelationLiteSDK.xcframework");
            bool bAvailable = Directory.Exists(SdkPath);
            PublicDefinitions.Add("TAP_RELATIONLITE_IOS_SDK_AVAILABLE=" + (bAvailable ? "1" : "0"));
            if (bAvailable)
            {
                // TapTapRelationLiteResource.bundle 承载好友 UI 图片资源，必须随 xcframework 一起拷入 app 包，
                // 否则 TTFBundleUtils 在运行时找不到 bundle 而崩溃（与 TapLeaderboard 同构问题）。
                string RelationLiteResourcePath = Path.Combine(IosThirdPartyRoot, "TapTapRelationLiteResource.bundle");
                PublicAdditionalFrameworks.Add(new Framework(
                    "TapTapRelationLiteSDK",
                    SdkPath,
                    Directory.Exists(RelationLiteResourcePath) ? RelationLiteResourcePath : null,
                    true));
                PublicIncludePaths.Add(Path.Combine(SdkPath, "ios-arm64", "TapTapRelationLiteSDK.framework", "Headers"));

                // TapTapSDKBridgeCore: TapRelationLite 私有依赖
                string BridgeCorePath = Path.Combine(IosThirdPartyRoot, "TapTapSDKBridgeCore.xcframework");
                if (Directory.Exists(BridgeCorePath))
                {
                    PublicAdditionalFrameworks.Add(new Framework("TapTapSDKBridgeCore", BridgeCorePath, "", true));
                }
                // TapTapKingfisher + TapTapProfileSDK 由 TapProfile 插件统一提供，此处不重复链接
            }
            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/iOS"));
        }
    }
}
