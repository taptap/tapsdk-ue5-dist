// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TapProfile : ModuleRules
{
    public TapProfile(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string ThirdPartyPath = Path.GetFullPath(
                Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));

            // TapTapKingfisher + TapTapProfileSDK are shared by TapRelation, TapRelationLite and
            // TapLeaderboard. Placing them here ensures each is embedded exactly once.
            string[] Frameworks = { "TapTapKingfisher", "TapTapProfileSDK" };
            foreach (string Fw in Frameworks)
            {
                string FwPath = Path.Combine(ThirdPartyPath, Fw + ".xcframework");
                if (Directory.Exists(FwPath))
                {
                    // TapTapProfileResource.bundle 承载好友/资料 UI 图片资源，随 TapTapProfileSDK 一起拷入 app 包，
                    // 否则 TapProfileBundleUtils 在运行时找不到 bundle 而崩溃（与 TapLeaderboard 同构问题）。
                    string ResourcePath = null;
                    if (Fw == "TapTapProfileSDK")
                    {
                        string ProfileResourcePath = Path.Combine(ThirdPartyPath, "TapTapProfileResource.bundle");
                        ResourcePath = Directory.Exists(ProfileResourcePath) ? ProfileResourcePath : null;
                    }
                    PublicAdditionalFrameworks.Add(new Framework(Fw, FwPath, ResourcePath, true));
                }
            }
        }
    }
}
