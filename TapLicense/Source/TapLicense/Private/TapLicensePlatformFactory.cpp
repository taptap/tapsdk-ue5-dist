// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapLicensePlatform.h"
#include "TapLicenseModule.h"

#if PLATFORM_WINDOWS
#include "Win64/TapLicensePlatformWin64.h"
#elif PLATFORM_ANDROID
#include "Mobile/Android/TapLicenseAndroid.h"
#elif PLATFORM_IOS
#include "Mobile/iOS/TapLicenseiOS.h"
#elif PLATFORM_MAC || PLATFORM_LINUX
#include "Platform/TapLicensePlatformStub.h"
#endif

TSharedPtr<ITapLicensePlatform> FTapLicensePlatformFactory::PlatformInstance = nullptr;

TSharedPtr<ITapLicensePlatform> FTapLicensePlatformFactory::CreatePlatform()
{
	if (PlatformInstance.IsValid())
	{
		return PlatformInstance;
	}

#if PLATFORM_WINDOWS
	PlatformInstance = MakeShared<FTapLicensePlatformWin64>();
#elif PLATFORM_ANDROID
	PlatformInstance = MakeShared<FTapLicenseAndroid>();
#elif PLATFORM_IOS
	PlatformInstance = MakeShared<FTapLicenseiOS>();
#elif PLATFORM_MAC || PLATFORM_LINUX
	PlatformInstance = MakeShared<FTapLicensePlatformStub>();
#else
	UE_LOG(LogTapLicense, Error, TEXT("TapLicense: 当前平台不支持"));
	return nullptr;
#endif

	if (PlatformInstance.IsValid())
	{
		UE_LOG(LogTapLicense, Log, TEXT("TapLicense platform created: %s"), *PlatformInstance->GetPlatformName());
	}
	return PlatformInstance;
}

TSharedPtr<ITapLicensePlatform> FTapLicensePlatformFactory::GetExisting()
{
	return PlatformInstance;
}

void FTapLicensePlatformFactory::ResetPlatform()
{
	PlatformInstance.Reset();
}
