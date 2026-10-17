// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TapCloudSave : ModuleRules
{
	public TapCloudSave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
		}
		bUseUnity = false;

		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Public")));
		PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "Private")));

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine",
			"Json", "JsonUtilities",
			"TapCore", "TapLogin"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"HTTP"
		});

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// Launch is required so the module sees FAndroidApplication helpers
			// (FAndroidApplication::GetJavaEnv / GetGameActivityThis) without
			// extra search-path gymnastics — matches every other Tap* plugin.
			PrivateDependencyModuleNames.Add("Launch");

			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				Path.Combine(ModuleDirectory, "TapCloudSave_Android_UPL.xml"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// Launch is required for any module that mixes ObjC++ with UE5 on iOS
			// (provides IOSAppDelegate forward declarations etc.).
			PrivateDependencyModuleNames.Add("Launch");

			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation" });

			AdditionalPropertiesForReceipt.Add(
				"IOSPlugin",
				Path.Combine(ModuleDirectory, "TapCloudSave_iOS_UPL.xml"));

			string IosThirdPartyRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));

			// Swift/ObjC bridge surface that we call directly from PlatformIOS.mm.
			string CloudSaveSdkPath = Path.Combine(IosThirdPartyRoot, "TapTapCloudSaveSDK.xcframework");
			PublicAdditionalFrameworks.Add(new Framework("TapTapCloudSaveSDK", CloudSaveSdkPath, "", true));

			// C++ core static archive that TapTapCloudSaveSDK links against. Must be
			// added to AdditionalFrameworks (not just included) so UBT copies it into
			// the build and the linker resolves all symbols.
			string NativeCoreSdkPath = Path.Combine(IosThirdPartyRoot, "cloudsave_sdk.xcframework");
			PublicAdditionalFrameworks.Add(new Framework("cloudsave_sdk", NativeCoreSdkPath, "", true));

			// Expose framework headers (BridgeCloudSaveService.h / -Swift.h). UBT
			// selects the slice that matches the current arch at build time.
			PublicIncludePaths.Add(Path.Combine(CloudSaveSdkPath, "ios-arm64", "TapTapCloudSaveSDK.framework", "Headers"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Desktop"));

			string ThirdPartyRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "Desktop"));
			PublicIncludePaths.Add(Path.Combine(ThirdPartyRoot, "Include"));

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				string DllPath = Path.Combine(ThirdPartyRoot, "Win64", "cloudsave_sdk.dll");
				if (File.Exists(DllPath))
				{
					// Delay-load so the DLL is loaded on demand at runtime; two-arg
					// RuntimeDependencies stages the file into $(BinaryOutputDir) so
					// the OS loader can resolve the short name in packaged builds.
					PublicDelayLoadDLLs.Add("cloudsave_sdk.dll");
					RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "cloudsave_sdk.dll"), DllPath);
				}
				else
				{
					System.Console.WriteLine("[TapCloudSave] Win64 cloudsave_sdk.dll missing at " + DllPath +
					                         " — desktop runtime will report InitFail.");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				// The Mac desktop target is supported only when the native dylib is
				// present. Failing here prevents a package that always reports 300002.
				string DylibPath = Path.Combine(ThirdPartyRoot, "Mac", "libcloudsave_sdk.dylib");
				if (File.Exists(DylibPath))
				{
					// PublicDelayLoadDLLs is Windows-only; on macOS we link via
					// PublicAdditionalLibraries and stage the dylib into
					// $(BinaryOutputDir) so dlopen(short-name) resolves at runtime.
					PublicAdditionalLibraries.Add(DylibPath);
					RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "libcloudsave_sdk.dylib"), DylibPath);
				}
				else
				{
					throw new BuildException("[TapCloudSave] Mac libcloudsave_sdk.dylib is required but missing at " + DylibPath +
					                         ". Fetch Git LFS assets before building the Mac target.");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				string SoPath = Path.Combine(ThirdPartyRoot, "Linux", "libcloudsave_sdk.so");
				if (File.Exists(SoPath))
				{
					// Same staging idiom as Mac: link via PublicAdditionalLibraries
					// and copy the .so next to the binary for dlopen lookup.
					PublicAdditionalLibraries.Add(SoPath);
					RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "libcloudsave_sdk.so"), SoPath);
				}
				else
				{
					System.Console.WriteLine("[TapCloudSave] Linux libcloudsave_sdk.so missing at " + SoPath +
					                         " — desktop runtime will report InitFail.");
				}
			}
		}
	}
}
