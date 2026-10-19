// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class TapRelation : ModuleRules
{
    public TapRelation(ReadOnlyTargetRules Target) : base(Target)
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
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "TapRelation_Android_UPL.xml"));
            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
        }

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.Add("Launch");
            PrivateDependencyModuleNames.Add("ApplicationCore"); // FIOSCoreDelegates::OnOpenURL
            PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation" });
            AdditionalPropertiesForReceipt.Add("IOSPlugin", Path.Combine(ModuleDirectory, "TapRelation_iOS_UPL.xml"));

            string IosThirdPartyRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
            string SdkPath = Path.Combine(IosThirdPartyRoot, "TapTapRelationSDK.xcframework");
            bool bAvailable = Directory.Exists(SdkPath);
            PublicDefinitions.Add("TAP_RELATION_IOS_SDK_AVAILABLE=" + (bAvailable ? "1" : "0"));
            if (bAvailable)
            {
                // TapTapRelationResource.bundle 承载好友 UI 图片资源，必须随 xcframework 一起拷入 app 包，
                // 否则 TTFBundleUtils 在运行时找不到 bundle 而崩溃（与 TapLeaderboard 同构问题）。
                string RelationResourcePath = Path.Combine(IosThirdPartyRoot, "TapTapRelationResource.bundle");
                PublicAdditionalFrameworks.Add(new Framework(
                    "TapTapRelationSDK",
                    SdkPath,
                    Directory.Exists(RelationResourcePath) ? RelationResourcePath : null,
                    true));
                PublicIncludePaths.Add(Path.Combine(SdkPath, "ios-arm64", "TapTapRelationSDK.framework", "Headers"));
                // BridgeRelationService.h 与 xcframework 并列放在 Frameworks 目录下
                PublicIncludePaths.Add(IosThirdPartyRoot);

                // bifrost_sdk + TapTapSDKBridgeCore: TapRelation 私有依赖
                string[] TransitiveFws = { "bifrost_sdk", "TapTapSDKBridgeCore" };
                foreach (string Fw in TransitiveFws)
                {
                    string FwPath = Path.Combine(IosThirdPartyRoot, Fw + ".xcframework");
                    if (Directory.Exists(FwPath))
                    {
                        PublicAdditionalFrameworks.Add(new Framework(Fw, FwPath, "", true));
                    }
                }
                // TapTapKingfisher + TapTapProfileSDK 由 TapProfile 插件统一提供，此处不重复链接
            }
            PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/iOS"));
        }
    }
}
