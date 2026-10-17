// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#endif

public class TapLogin : ModuleRules
{
	public TapLogin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.IOS ||
		    Target.Platform == UnrealTargetPlatform.Android)
		{
			// The module mixes C++ and Objective-C++ (.mm) sources.
			// On Apple platforms (Mac/iOS) and Android (cross-compiled on Mac with clang),
			// a shared PCH built without ObjC support will conflict when a .mm file is compiled.
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
				"Sockets",
				"Networking",
				"TapCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"RenderCore",
				"InputCore",
				"UMG"
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
					"SafariServices",
					"AuthenticationServices",
					"WebKit"
				}
			);

			string ThirdPartyPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));

			// Add TapTapLoginSDK xcframework with its bundle resource
			PublicAdditionalFrameworks.Add(
				new Framework(
					"TapTapLoginSDK",
					Path.Combine(ThirdPartyPath, "TapTapLoginSDK.xcframework"),
					Path.Combine(ThirdPartyPath, "TapTapLoginResource.bundle"),
					true  // bCopyFramework
				)
			);

			// Add header include paths from xcframework
			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "TapTapLoginSDK.xcframework", "ios-arm64", "TapTapLoginSDK.framework", "Headers"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
		}

		// Android platform configuration
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Add Launch module for Android JNI support
			PrivateDependencyModuleNames.Add("Launch");

			// Add Android UPL configuration (using remote Maven dependency)
			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				Path.Combine(ModuleDirectory, "TapLogin_Android_UPL.xml")
			);

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
		}

		// Desktop platform configuration
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Platform")));
			PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/Desktop")));
			PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private/ThirdParty/QrCodeGen")));
			PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "TapCore", "Source", "ThirdParty", "TapLauncher", "Include")));

			PrivateDependencyModuleNames.Add("ImageWrapper");

			// Stage sprite resources for packaged builds
			string SpritesDir = Path.Combine(PluginDirectory, "Resources", "Sprites");
			if (Directory.Exists(SpritesDir))
			{
				foreach (string File in Directory.GetFiles(SpritesDir, "*.png"))
				{
					RuntimeDependencies.Add(File);
				}
			}
		}
	}
}
