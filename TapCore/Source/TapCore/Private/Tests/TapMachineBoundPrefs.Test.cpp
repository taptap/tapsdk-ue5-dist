// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/Storage/TapPrefs.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapMachineBoundPrefsInvalidationTest,
	"TapSDK.TapBasic.Storage.MachineBoundPrefs.InvalidatesCopiedCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapMachineBoundPrefsInvalidationTest::RunTest(const FString&)
{
	const FString RelativePath = FString::Printf(
		TEXT("Automation/TapMachineBoundPrefs_%s.json"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString FullPath = FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath);

	FTapPrefs CopiedPrefs;
	CopiedPrefs.Init(RelativePath);
	CopiedPrefs.Set(TEXT("tap_machine_fp"), TEXT("copied-from-another-machine"));
	CopiedPrefs.Set(TEXT("token"), TEXT("copied-token"));
	CopiedPrefs.Save();

	FTapMachineBoundPrefs BoundPrefs;
	BoundPrefs.Init(RelativePath);
	TestTrue(TEXT("A copied cache is invalidated"), BoundPrefs.WasInvalidated());
	TestTrue(TEXT("Copied token data is cleared"), BoundPrefs.GetString(TEXT("token")).IsEmpty());

	BoundPrefs.Set(TEXT("token"), TEXT("local-token"));
	BoundPrefs.Save();
	BoundPrefs.Clear();
	BoundPrefs.Save();

	FTapMachineBoundPrefs ReloadedPrefs;
	ReloadedPrefs.Init(RelativePath);
	TestFalse(TEXT("A cache saved on this machine remains valid"), ReloadedPrefs.WasInvalidated());
	TestTrue(TEXT("Clear removes token data"), ReloadedPrefs.GetString(TEXT("token")).IsEmpty());

	IFileManager::Get().Delete(*FullPath, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && desktop
