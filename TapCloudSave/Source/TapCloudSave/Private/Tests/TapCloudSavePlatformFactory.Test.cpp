// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ITapCloudSavePlatform.h"

/**
 * Asserts that FTapCloudSavePlatformFactory::CreatePlatform() returns the
 * platform implementation whose GetPlatformName() matches the current build
 * target.
 *
 * The factory guards itself with #if PLATFORM_WINDOWS / PLATFORM_IOS /
 * PLATFORM_ANDROID branches; the assertion below mirrors those compile-time
 * gates so the right name is checked per platform.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCloudSaveFactoryDispatchTest,
	"TapSDK.CloudSave.Factory.Dispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapCloudSaveFactoryDispatchTest::RunTest(const FString& /*Parameters*/)
{
	// Make sure no stray test double from a previous test leaks into this one.
	FTapCloudSavePlatformFactory::SetPlatformForTest(nullptr);

	TSharedPtr<ITapCloudSavePlatform> Platform = FTapCloudSavePlatformFactory::CreatePlatform();
	TestTrue(TEXT("Factory returns a valid platform instance for this target"),
		Platform.IsValid());
	if (!Platform.IsValid()) { return false; }

#if PLATFORM_IOS
	TestEqual(TEXT("iOS target dispatches to PlatformIOS"),
		Platform->GetPlatformName(), FString(TEXT("iOS")));
#elif PLATFORM_ANDROID
	TestEqual(TEXT("Android target dispatches to PlatformAndroid"),
		Platform->GetPlatformName(), FString(TEXT("Android")));
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	TestEqual(TEXT("Desktop target (Win/Mac/Linux) dispatches to PlatformDesktop"),
		Platform->GetPlatformName(), FString(TEXT("Desktop")));
#endif

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
