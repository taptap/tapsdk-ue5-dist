// Copyright TapTap. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TapBattle : ModuleRules
{
	public TapBattle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			// Mixed C++/ObjC++ — disable PCH so .mm sources don't pick up a C++-only PCH.
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
			PrivateDependencyModuleNames.Add("Launch");

			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				Path.Combine(ModuleDirectory, "TapBattle_Android_UPL.xml"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/Android"));
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PrivateDependencyModuleNames.Add("Launch");

			PublicFrameworks.AddRange(new string[] { "UIKit", "Foundation" });

			AdditionalPropertiesForReceipt.Add(
				"IOSPlugin",
				Path.Combine(ModuleDirectory, "TapBattle_iOS_UPL.xml"));

			string IosThirdPartyRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "ThirdParty", "iOS", "Frameworks"));

			// Unity OnlineBattle calls the native C ABI directly. Mirror that path on
			// UE iOS: link only onlinebattle_sdk.xcframework and call TapSdkOnlineBattle*.
			string OnlineBattleSdkPath = Path.Combine(IosThirdPartyRoot, "onlinebattle_sdk.xcframework");
			PublicAdditionalFrameworks.Add(new Framework("onlinebattle_sdk", OnlineBattleSdkPath, "", true));
			PublicIncludePaths.Add(Path.Combine(OnlineBattleSdkPath, "ios-arm64", "onlinebattle_sdk.framework", "Headers"));

			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Mobile/iOS"));
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Mac ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Platform"));
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Desktop"));

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				string Win64Root = Path.GetFullPath(Path.Combine(ModuleDirectory, "x64"));
				string OnlineBattleDll = Path.Combine(Win64Root, "onlinebattle_sdk.dll");
				if (File.Exists(OnlineBattleDll))
				{
					foreach (string DllPath in Directory.GetFiles(Win64Root, "*.dll"))
					{
						string DllName = Path.GetFileName(DllPath);
						// TapCore/TapCloudSave stage shared SDK DLLs to the same binary output path.
						// Avoid duplicate RuntimeDependencies with different source paths.
						if (DllName == "tapsdkcore.dll" || DllName == "cloudsave_sdk.dll")
						{
							continue;
						}
						RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", DllName), DllPath);
					}
				}
				else
				{
					System.Console.WriteLine("[TapBattle] Win64 onlinebattle_sdk.dll missing at " + OnlineBattleDll +
					                         " — desktop runtime will report InitFail.");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				string DylibPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "libonlinebattle_sdk.dylib"));
				if (File.Exists(DylibPath))
				{
					RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "libonlinebattle_sdk.dylib"), DylibPath);
				}
				else
				{
					System.Console.WriteLine("[TapBattle] Mac libonlinebattle_sdk.dylib missing at " + DylibPath +
					                         " — desktop runtime will report InitFail.");
				}
			}
		}
	}
}
