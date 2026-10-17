// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapMomentPlatform.h"
#include "TapMomentModule.h"

#if PLATFORM_ANDROID
#include "Android/TapMomentPlatformAndroid.h"
#elif PLATFORM_IOS
#include "iOS/TapMomentPlatformIOS.h"
#endif

TSharedPtr<ITapMomentPlatform> FTapMomentPlatformFactory::PlatformInstance = nullptr;

void FTapMomentPlatformFactory::Reset()
{
    PlatformInstance.Reset();
}

TSharedPtr<ITapMomentPlatform> FTapMomentPlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }

#if PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapMomentPlatformAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapMomentPlatformIOS>();
#else
    UE_LOG(LogTapMoment, Warning, TEXT("TapMoment: no platform implementation for this target"));
#endif

    if (PlatformInstance.IsValid())
    {
        UE_LOG(LogTapMoment, Log, TEXT("TapMoment platform: %s"), *PlatformInstance->GetPlatformName());
    }
    return PlatformInstance;
}
