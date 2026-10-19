// Copyright TapTap. All Rights Reserved.

#include "Platform/ITapLoginPlatform.h"
#include "TapLoginModule.h"

// 包含各平台实现
#if PLATFORM_ANDROID
#include "Platform/TapLoginPlatformAndroid.h"
#elif PLATFORM_IOS
#include "Platform/TapLoginPlatformIOS.h"
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Platform/TapLoginPlatformDesktop.h"
#endif

// 初始化静态成员
TSharedPtr<ITapLoginPlatform> FTapLoginPlatformFactory::PlatformInstance = nullptr;

TSharedPtr<ITapLoginPlatform> FTapLoginPlatformFactory::CreatePlatform()
{
	// 单例模式：只创建一次
	if (PlatformInstance.IsValid())
	{
		return PlatformInstance;
	}

	UE_LOG(LogTapLogin, Log, TEXT("Creating platform-specific TapLogin implementation..."));

#if PLATFORM_ANDROID
	UE_LOG(LogTapLogin, Log, TEXT("Platform: Android (JNI Bridge)"));
	PlatformInstance = MakeShared<FTapLoginPlatformAndroid>();
#elif PLATFORM_IOS
	UE_LOG(LogTapLogin, Log, TEXT("Platform: iOS (Objective-C++ Bridge)"));
	PlatformInstance = MakeShared<FTapLoginPlatformIOS>();
#elif PLATFORM_WINDOWS
	UE_LOG(LogTapLogin, Log, TEXT("Platform: Windows (Native OAuth)"));
	PlatformInstance = MakeShared<FTapLoginPlatformDesktop>();
#elif PLATFORM_MAC
	UE_LOG(LogTapLogin, Log, TEXT("Platform: Mac (Native OAuth)"));
	PlatformInstance = MakeShared<FTapLoginPlatformDesktop>();
#elif PLATFORM_LINUX
	UE_LOG(LogTapLogin, Log, TEXT("Platform: Linux (Native OAuth)"));
	PlatformInstance = MakeShared<FTapLoginPlatformDesktop>();
#else
	UE_LOG(LogTapLogin, Error, TEXT("Platform not supported for TapLogin"));
	return nullptr;
#endif

	if (PlatformInstance.IsValid())
	{
		UE_LOG(LogTapLogin, Log, TEXT("Platform implementation created: %s"), *PlatformInstance->GetPlatformName());
	}

	return PlatformInstance;
}
