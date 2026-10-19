// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapSharePlatform.h"
#include "TapShareModule.h"

#if PLATFORM_ANDROID
#include "Android/TapSharePlatformAndroid.h"
#elif PLATFORM_IOS
#include "iOS/TapSharePlatformIOS.h"
#endif

TSharedPtr<ITapSharePlatform> FTapSharePlatformFactory::PlatformInstance = nullptr;

void FTapSharePlatformFactory::Reset()
{
    PlatformInstance.Reset();
}

TSharedPtr<ITapSharePlatform> FTapSharePlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }

#if PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapSharePlatformAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapSharePlatformIOS>();
#else
    UE_LOG(LogTapShare, Warning, TEXT("TapShare: no platform implementation for this target"));
#endif

    if (PlatformInstance.IsValid())
    {
        UE_LOG(LogTapShare, Log, TEXT("TapShare platform: %s"), *PlatformInstance->GetPlatformName());
    }
    return PlatformInstance;
}
