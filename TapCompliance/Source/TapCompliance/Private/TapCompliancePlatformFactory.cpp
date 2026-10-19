// Copyright TapTap. All Rights Reserved.

#include "Platform/ITapCompliancePlatform.h"
#include "TapCompliance.h"
#include "TapComplianceModule.h"

// 包含各平台实现
#if PLATFORM_ANDROID
#include "Platform/TapComplianceAndroid.h"
#elif PLATFORM_IOS
#include "Platform/TapComplianceiOS.h"
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Platform/TapCompliancePlatformDesktop.h"
#endif

// 初始化静态成员
TSharedPtr<ITapCompliancePlatform> FTapCompliancePlatformFactory::PlatformInstance = nullptr;

TSharedPtr<ITapCompliancePlatform> FTapCompliancePlatformFactory::CreatePlatform()
{
	// 单例模式：只创建一次
	if (PlatformInstance.IsValid())
	{
		return PlatformInstance;
	}

	UE_LOG(LogTapCompliance, Log, TEXT("Creating platform-specific TapCompliance implementation..."));

#if PLATFORM_ANDROID
	UE_LOG(LogTapCompliance, Log, TEXT("Platform: Android (JNI Bridge)"));
	PlatformInstance = MakeShared<FTapComplianceAndroid>();
#elif PLATFORM_IOS
	UE_LOG(LogTapCompliance, Log, TEXT("Platform: iOS (ObjC++ Bridge)"));
	PlatformInstance = MakeShared<FTapComplianceiOS>();
#elif PLATFORM_WINDOWS
	UE_LOG(LogTapCompliance, Log, TEXT("Platform: Windows (Desktop Stub)"));
	PlatformInstance = MakeShared<FTapCompliancePlatformDesktop>();
#elif PLATFORM_MAC
	UE_LOG(LogTapCompliance, Log, TEXT("Platform: Mac (Desktop Stub)"));
	PlatformInstance = MakeShared<FTapCompliancePlatformDesktop>();
#elif PLATFORM_LINUX
	UE_LOG(LogTapCompliance, Log, TEXT("Platform: Linux (Desktop Stub)"));
	PlatformInstance = MakeShared<FTapCompliancePlatformDesktop>();
#else
	UE_LOG(LogTapCompliance, Error, TEXT("Platform not supported for TapCompliance"));
	return nullptr;
#endif

	if (PlatformInstance.IsValid())
	{
		UE_LOG(LogTapCompliance, Log, TEXT("Platform implementation created: %s"), *PlatformInstance->GetPlatformName());
	}

	return PlatformInstance;
}
