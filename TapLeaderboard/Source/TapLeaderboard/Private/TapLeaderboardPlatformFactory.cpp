// Copyright TapTap. All Rights Reserved.
#include "Platform/ITapLeaderboardPlatform.h"
#include "TapLeaderboardModule.h"

#if PLATFORM_ANDROID
#include "Mobile/Android/TapLeaderboardAndroid.h"
#elif PLATFORM_IOS
#include "Mobile/iOS/TapLeaderboardiOS.h"
#endif

TSharedPtr<ITapLeaderboardPlatform> FTapLeaderboardPlatformFactory::PlatformInstance = nullptr;

void FTapLeaderboardPlatformFactory::Reset()
{
    PlatformInstance.Reset();
}

TSharedPtr<ITapLeaderboardPlatform> FTapLeaderboardPlatformFactory::CreatePlatform()
{
    if (PlatformInstance.IsValid()) { return PlatformInstance; }

#if PLATFORM_ANDROID
    PlatformInstance = MakeShared<FTapLeaderboardAndroid>();
#elif PLATFORM_IOS
    PlatformInstance = MakeShared<FTapLeaderboardiOS>();
#else
    UE_LOG(LogTapLeaderboard, Warning, TEXT("TapLeaderboard is mobile-only; not supported on this platform."));
#endif

    if (PlatformInstance.IsValid())
    {
        UE_LOG(LogTapLeaderboard, Log, TEXT("TapLeaderboard platform: %s"), *PlatformInstance->GetPlatformName());
    }
    return PlatformInstance;
}
