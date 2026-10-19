// Copyright TapTap. All Rights Reserved.
#include "Desktop/TapAchievementPlatformDesktop.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "TapAchievementDesktop.h"

void FTapAchievementPlatformDesktop::Initialize(const FTapTapSdkOptions& Config)
{
    FTapAchievementDesktop::Get().Initialize(Config);
}

void FTapAchievementPlatformDesktop::Unlock(const FString& AchievementId)
{
    FTapAchievementDesktop::Get().Unlock(AchievementId);
}

void FTapAchievementPlatformDesktop::Increment(const FString& AchievementId, int32 Step)
{
    FTapAchievementDesktop::Get().Increment(AchievementId, Step);
}

void FTapAchievementPlatformDesktop::ShowAchievements()
{
    FTapAchievementDesktop::Get().ShowAchievements();
}

void FTapAchievementPlatformDesktop::SetToastEnable(bool bEnable)
{
    FTapAchievementDesktop::Get().SetToastEnable(bEnable);
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC
