// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapRelationPlatform.h"
#include "TapRelationModule.h"

#if PLATFORM_ANDROID
#include "Android/TapRelationPlatformAndroid.h"
#elif PLATFORM_IOS
#include "iOS/TapRelationPlatformIOS.h"
#endif

TSharedPtr<ITapRelationPlatform> FTapRelationPlatformFactory::PlatformInstance = nullptr;

void FTapRelationPlatformFactory::Reset() { PlatformInstance.Reset(); }

TSharedPtr<ITapRelationPlatform> FTapRelationPlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }
#if PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapRelationPlatformAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapRelationPlatformIOS>();
#else
    UE_LOG(LogTapRelation, Warning, TEXT("TapRelation: no platform implementation for this target"));
#endif
    if (PlatformInstance.IsValid())
        UE_LOG(LogTapRelation, Log, TEXT("TapRelation platform: %s"), *PlatformInstance->GetPlatformName());
    return PlatformInstance;
}
