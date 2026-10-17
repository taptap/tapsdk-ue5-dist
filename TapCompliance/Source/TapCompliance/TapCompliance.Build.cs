// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#endif

public class TapCompliance : ModuleRules
{
	public TapCompliance(ReadOnlyTargetRules Target) : base(Target)
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

		// Include paths
		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Public")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private")));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TapLogin",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"TapCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"HTTP",
			}
		);

		// Android platform configuration
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Add Launch module for Android JNI support
			PrivateDependencyModuleNames.Add("Launch");

			// Add Android UPL configuration (using remote Maven dependency)
			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				Path.Combine(ModuleDirectory, "TapCompliance_Android_UPL.xml")
			);

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
		}

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
					"SystemConfiguration"
				}
			);

			// Add iOS UPL configuration
			AdditionalPropertiesForReceipt.Add(
				"IOSPlugin",
				Path.Combine(ModuleDirectory, "TapCompliance_iOS_UPL.xml")
			);

			// Path to xcframeworks
			string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));

			// Add TapTapComplianceSDK xcframework with its bundle resource
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapComplianceSDK",
					Path.Combine(ThirdPartyPath, "TapTapComplianceSDK.xcframework"),
					Path.Combine(ThirdPartyPath, "TapTapComplianceResource.bundle"),
					true  // bCopyFramework
				)
			);

			// Add header include paths from xcframework
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "TapTapComplianceSDK.xcframework", "ios-arm64", "TapTapComplianceSDK.framework", "Headers"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
		}

		// Desktop platform configuration
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Desktop"));
			// Automation 单测文件目录，让 Tests/ 下的 cpp 能互相 include 同级头
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Tests"));

			PrivateDependencyModuleNames.Add("UMG");
		}
	}
}
