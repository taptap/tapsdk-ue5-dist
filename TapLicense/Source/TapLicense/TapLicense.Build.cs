// Copyright TapTap. All Rights Reserved.
using UnrealBuildTool;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#endif

public class TapLicense : ModuleRules
{
	public TapLicense(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.IOS ||
		    Target.Platform == UnrealTargetPlatform.Android)
		{
			// .mm 与 .cpp 混编，关闭 PCH 避免 ObjC++ 复用非 ObjC PCH
			PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		}
		bUseUnity = false;

		// Read plugin version from .uplugin file
		FileReference fileRef = new FileReference(Path.Combine(PluginDirectory, Name + ".uplugin"));
		PluginInfo plugin = new PluginInfo(fileRef, PluginType.Project);
		PublicDefinitions.Add(Name + "_VERSION=TEXT(\"" + plugin.Descriptor.VersionName + "\")");

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Public")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private")));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "TapCore"
		});

		// 公共平台抽象目录
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Platform")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Platform/Tests")));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Win64")));
			PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Win64/Tests")));
			// Reuse TapCore's taptap_api includes via TapLauncherDllWrapper
			string LauncherThirdParty = Path.GetFullPath(
				Path.Combine(ModuleDirectory, "..", "..", "..", "TapCore", "Source", "ThirdParty", "TapLauncher"));
			PrivateIncludePaths.Add(Path.Combine(LauncherThirdParty, "Include"));
		}
		// Mac/Linux: 仅编译 Private/Platform 下的 stub，无额外依赖

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				Path.Combine(ModuleDirectory, "TapLicense_Android_UPL.xml"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PrivateDependencyModuleNames.Add("Launch");
			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation", "SystemConfiguration" });
			AdditionalPropertiesForReceipt.Add(
				"IOSPlugin",
				Path.Combine(ModuleDirectory, "TapLicense_iOS_UPL.xml"));

			string ThirdPartyPath = Path.GetFullPath(
				Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));
			// VERIFY: framework 名以实际交付的 xcframework 为准
			// xcframework 由原生团队单独交付；未安装时跳过注册，避免 UBT 读 Info.plist 崩溃。
			string LicenseSdkPath = Path.Combine(ThirdPartyPath, "TapTapLicenseSDK.xcframework");
			if (Directory.Exists(LicenseSdkPath))
			{
				PublicAdditionalFrameworks.Add(new Framework(
					"TapTapLicenseSDK",
					LicenseSdkPath,
					"",
					true));
				PublicIncludePaths.Add(Path.Combine(LicenseSdkPath,
					"ios-arm64", "TapTapLicenseSDK.framework", "Headers"));
			}
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
		}
	}
}
