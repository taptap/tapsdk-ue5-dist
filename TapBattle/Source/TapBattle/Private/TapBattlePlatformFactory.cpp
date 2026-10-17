// Copyright TapTap. All Rights Reserved.

#include "Platform/ITapBattlePlatform.h"
#include "TapBattleModule.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Platform/TapBattlePlatformDesktop.h"
#elif PLATFORM_ANDROID
#include "Platform/TapBattlePlatformAndroid.h"
#elif PLATFORM_IOS
#include "Mobile/iOS/TapBattlePlatformIOS.h"
#endif

TSharedPtr<ITapBattlePlatform> FTapBattlePlatformFactory::PlatformInstance = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
TSharedPtr<ITapBattlePlatform> FTapBattlePlatformFactory::TestInstance     = nullptr;

void FTapBattlePlatformFactory::SetPlatformForTest(TSharedPtr<ITapBattlePlatform> InTest)
{
	TestInstance = InTest;
}

#endif

void FTapBattlePlatformFactory::Reset()
{
	// Drop production platform so its destructor releases any native handle
	// (mirrors TapCloudSavePlatformFactory::Reset). TestInstance owned by harness.
	PlatformInstance.Reset();
}

TSharedPtr<ITapBattlePlatform> FTapBattlePlatformFactory::CreatePlatform()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestInstance.IsValid())     { return TestInstance; }
#endif
	if (PlatformInstance.IsValid()) { return PlatformInstance; }

	UE_LOG(LogTapBattle, Log, TEXT("Creating platform-specific TapBattle implementation..."));

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	UE_LOG(LogTapBattle, Log, TEXT("Platform: Desktop"));
	PlatformInstance = MakeShared<FTapBattlePlatformDesktop>();
#elif PLATFORM_ANDROID
	UE_LOG(LogTapBattle, Log, TEXT("Platform: Android"));
	PlatformInstance = MakeShared<FTapBattlePlatformAndroid>();
#elif PLATFORM_IOS
	UE_LOG(LogTapBattle, Log, TEXT("Platform: iOS"));
	PlatformInstance = MakeShared<FTapBattlePlatformIOS>();
#else
	UE_LOG(LogTapBattle, Warning, TEXT("TapBattle platform not yet implemented for this target"));
#endif
	return PlatformInstance;
}
