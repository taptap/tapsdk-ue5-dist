// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_ANDROID

#include "Platform/ITapAchievementPlatform.h"

class FTapAchievementPlatformAndroid : public ITapAchievementPlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Unlock(const FString& AchievementId) override;
    virtual void Increment(const FString& AchievementId, int32 Step) override;
    virtual void ShowAchievements() override;
    virtual void SetToastEnable(bool bEnable) override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }
};

#endif // PLATFORM_ANDROID
