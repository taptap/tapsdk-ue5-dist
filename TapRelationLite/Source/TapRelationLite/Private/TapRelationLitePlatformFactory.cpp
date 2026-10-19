// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapRelationLitePlatform.h"
#include "TapRelationLiteModule.h"

#if PLATFORM_ANDROID
#include "Android/TapRelationLitePlatformAndroid.h"
#elif PLATFORM_IOS
#include "iOS/TapRelationLitePlatformIOS.h"
#endif

TSharedPtr<ITapRelationLitePlatform> FTapRelationLitePlatformFactory::PlatformInstance = nullptr;

void FTapRelationLitePlatformFactory::Reset() { PlatformInstance.Reset(); }

TSharedPtr<ITapRelationLitePlatform> FTapRelationLitePlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }
#if PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapRelationLitePlatformAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapRelationLitePlatformIOS>();
#else
    UE_LOG(LogTapRelationLite, Warning, TEXT("TapRelationLite: no platform implementation"));
#endif
    if (PlatformInstance.IsValid())
        UE_LOG(LogTapRelationLite, Log, TEXT("TapRelationLite platform: %s"), *PlatformInstance->GetPlatformName());
    return PlatformInstance;
}
