// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "Platform/ITapAchievementPlatform.h"

/**
 * 桌面平台实现：薄封装现有 FTapAchievementDesktop(HTTP + 离线队列 + WBP toast)。
 * 业务逻辑全部保留在 FTapAchievementDesktop，此处仅适配工厂接口。
 */
class FTapAchievementPlatformDesktop : public ITapAchievementPlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Unlock(const FString& AchievementId) override;
    virtual void Increment(const FString& AchievementId, int32 Step) override;
    virtual void ShowAchievements() override;
    virtual void SetToastEnable(bool bEnable) override;
    virtual FString GetPlatformName() const override { return TEXT("Desktop"); }
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC
