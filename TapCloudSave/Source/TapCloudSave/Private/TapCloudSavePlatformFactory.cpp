// Copyright TapTap. All Rights Reserved.

#include "Platform/ITapCloudSavePlatform.h"
#include "TapCloudSaveModule.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Platform/TapCloudSavePlatformDesktop.h"
#elif PLATFORM_ANDROID
#include "Platform/TapCloudSavePlatformAndroid.h"
#elif PLATFORM_IOS
#include "Mobile/iOS/TapCloudSavePlatformIOS.h"
#endif

TSharedPtr<ITapCloudSavePlatform> FTapCloudSavePlatformFactory::PlatformInstance = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
TSharedPtr<ITapCloudSavePlatform> FTapCloudSavePlatformFactory::TestInstance     = nullptr;

void FTapCloudSavePlatformFactory::SetPlatformForTest(TSharedPtr<ITapCloudSavePlatform> InTest)
{
	TestInstance = InTest;
}

#endif

void FTapCloudSavePlatformFactory::Reset()
{
	// Release the production platform instance — its destructor unloads the
	// native DLL via TapCloudSaveDesktopWrapper::Shutdown(). TestInstance is
	// owned by the test harness, so leave it alone.
	PlatformInstance.Reset();
}

TSharedPtr<ITapCloudSavePlatform> FTapCloudSavePlatformFactory::CreatePlatform()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestInstance.IsValid())     { return TestInstance; }
#endif
	if (PlatformInstance.IsValid()) { return PlatformInstance; }

	UE_LOG(LogTapCloudSave, Log, TEXT("Creating platform-specific TapCloudSave implementation..."));

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	UE_LOG(LogTapCloudSave, Log, TEXT("Platform: Desktop"));
	PlatformInstance = MakeShared<FTapCloudSavePlatformDesktop>();
#elif PLATFORM_ANDROID
	UE_LOG(LogTapCloudSave, Log, TEXT("Platform: Android"));
	PlatformInstance = MakeShared<FTapCloudSavePlatformAndroid>();
#elif PLATFORM_IOS
	UE_LOG(LogTapCloudSave, Log, TEXT("Platform: iOS"));
	PlatformInstance = MakeShared<FTapCloudSavePlatformIOS>();
#else
	UE_LOG(LogTapCloudSave, Warning, TEXT("TapCloudSave platform not yet implemented for this target"));
#endif
	return PlatformInstance;
}
