// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapAchievementPlatform.h"
#include "TapAchievementModule.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC
#include "Desktop/TapAchievementPlatformDesktop.h"
#elif PLATFORM_ANDROID
#include "Mobile/Android/TapAchievementPlatformAndroid.h"
#elif PLATFORM_IOS
#include "Mobile/iOS/TapAchievementPlatformIOS.h"
#endif

TSharedPtr<ITapAchievementPlatform> FTapAchievementPlatformFactory::PlatformInstance = nullptr;

void FTapAchievementPlatformFactory::Reset()
{
    PlatformInstance.Reset();
}

TSharedPtr<ITapAchievementPlatform> FTapAchievementPlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }

#if PLATFORM_WINDOWS || PLATFORM_MAC
    PlatformInstance = MakeShared<FTapAchievementPlatformDesktop>();
#elif PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapAchievementPlatformAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapAchievementPlatformIOS>();
#else
    UE_LOG(LogTapAchievement, Warning, TEXT("TapAchievement platform not implemented for this target"));
#endif

    if (PlatformInstance.IsValid())
    {
        UE_LOG(LogTapAchievement, Log, TEXT("TapAchievement platform: %s"), *PlatformInstance->GetPlatformName());
    }
    return PlatformInstance;
}
