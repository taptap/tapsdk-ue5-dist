// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapRepPlatform.h"
#include "TapRepModule.h"

#if PLATFORM_ANDROID
#include "Android/TapRepPlatformAndroid.h"
#elif PLATFORM_IOS
#include "iOS/TapRepPlatformIOS.h"
#endif

TSharedPtr<ITapRepPlatform> FTapRepPlatformFactory::PlatformInstance = nullptr;

void FTapRepPlatformFactory::Reset()
{
    PlatformInstance.Reset();
}

TSharedPtr<ITapRepPlatform> FTapRepPlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }

#if PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapRepPlatformAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapRepPlatformIOS>();
#else
    UE_LOG(LogTapRep, Warning, TEXT("TapRep: no platform implementation for this target"));
#endif

    if (PlatformInstance.IsValid())
    {
        UE_LOG(LogTapRep, Log, TEXT("TapRep platform: %s"), *PlatformInstance->GetPlatformName());
    }
    return PlatformInstance;
}
