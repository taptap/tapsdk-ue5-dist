// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#endif

public class TapCore : ModuleRules
{
	public TapCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			// The module mixes C++ and Objective-C++ sources on Apple platforms.
			// Disable PCH there to avoid ".mm" files reusing a non-ObjC PCH.
			PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		}

		// Disable Unity Build for platform-specific files to avoid compilation conflicts
		bUseUnity = false;

		// Read plugin version from .uplugin file
		FileReference fileRef = new FileReference(Path.Combine(PluginDirectory, Name + ".uplugin"));
		PluginInfo plugin = new PluginInfo(fileRef, PluginType.Project);
		PublicDefinitions.Add(Name + "_VERSION=TEXT(\"" + plugin.Descriptor.VersionName + "\")");

		// Include paths
		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Public")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Http")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Storage")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Event")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Tests")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"HTTP",
				"ApplicationCore",
				"PlatformCryptoContext",
				"UMG"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
			}
		);

		// iOS platform configuration
		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// Add Launch module for iOS support
			PrivateDependencyModuleNames.Add("Launch");

			PublicFrameworks.AddRange(
				new string[]
				{
					"UIKit",
					"Foundation",
					"SystemConfiguration",
					"Security"
				}
			);

			// Add iOS UPL configuration
			AdditionalPropertiesForReceipt.Add(
				"IOSPlugin",
				Path.Combine(ModuleDirectory, "TapCore_iOS_UPL.xml")
			);

			// Path to xcframeworks
			string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));

			// Add TapTapCoreSDK xcframework and its dependencies
			PublicAdditionalFrameworks.Add(
				new Framework(
					"THEMISLite",
					Path.Combine(ThirdPartyPath, "THEMISLite.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapBasicToolsSDK",
					Path.Combine(ThirdPartyPath, "TapTapBasicToolsSDK.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapCoreSDK",
					Path.Combine(ThirdPartyPath, "TapTapCoreSDK.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapGidSDK",
					Path.Combine(ThirdPartyPath, "TapTapGidSDK.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapNetworkSDK",
					Path.Combine(ThirdPartyPath, "TapTapNetworkSDK.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapSDKBridgeCore",
					Path.Combine(ThirdPartyPath, "TapTapSDKBridgeCore.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			PublicAdditionalFrameworks.Add(
				new Framework(
					"tapsdkcorecpp",
					Path.Combine(ThirdPartyPath, "tapsdkcorecpp.xcframework"),
					null,
					true  // bCopyFramework
				)
			);
			// Add header include paths from xcframework
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "TapTapCoreSDK.xcframework", "ios-arm64", "TapTapCoreSDK.framework", "Headers"));
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "TapTapBasicToolsSDK.xcframework", "ios-arm64", "TapTapBasicToolsSDK.framework", "Headers"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
		}

		// Android platform configuration
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Add Launch module for Android JNI support
			PrivateDependencyModuleNames.Add("Launch");

			// Add Android UPL configuration (using remote Maven dependency)
			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				Path.Combine(ModuleDirectory, "TapCore_Android_UPL.xml")
			);

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
		}

		// Desktop platform configuration
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Desktop"));
			PrivateDependencyModuleNames.Add("Projects");

			// ThirdParty native library headers (tapsdkcore)
			string DesktopThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "Desktop"));
			PrivateIncludePaths.Add(Path.Combine(DesktopThirdPartyPath, "Include"));

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				string DllPath = Path.Combine(DesktopThirdPartyPath, "Win64", "tapsdkcore.dll");
				// Delay-load so the DLL is loaded on demand at runtime
				PublicDelayLoadDLLs.Add("tapsdkcore.dll");
				RuntimeDependencies.Add("$(BinaryOutputDir)/tapsdkcore.dll", DllPath);

				// taptap_api — PC launcher verification (Win64 only)
				string LauncherThirdParty = Path.GetFullPath(
					Path.Combine(ModuleDirectory, "..", "ThirdParty", "TapLauncher"));
				PrivateIncludePaths.Add(Path.Combine(LauncherThirdParty, "Include"));
				string LauncherDllPath = Path.Combine(LauncherThirdParty, "Win64", "taptap_api.dll");
				// DLL is loaded at runtime via FPlatformProcess::GetDllHandle — no import lib needed
				RuntimeDependencies.Add("$(BinaryOutputDir)/taptap_api.dll", LauncherDllPath);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string DylibPath = Path.Combine(DesktopThirdPartyPath, "Mac", "libtapsdkcorecpp.dylib");
				PublicAdditionalLibraries.Add(DylibPath);
				RuntimeDependencies.Add("$(BinaryOutputDir)/libtapsdkcorecpp.dylib", DylibPath);
			}
		}
	}
}
