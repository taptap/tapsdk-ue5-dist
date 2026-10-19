// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"

/**
 * 成就平台抽象接口。
 *
 * 桌面(Win64/Mac)走自实现 HTTP + WBP toast；移动端(Android/iOS)经
 * 通用 EngineBridge 转发到原生 TapSDK，toast 由原生 SDK 自动弹出。
 * 各平台实现收到结果后交给内部 Callback Manager 分发。
 */
class TAPACHIEVEMENT_API ITapAchievementPlatform
{
public:
    virtual ~ITapAchievementPlatform() = default;

    virtual void Initialize(const FTapTapSdkOptions& Config) = 0;
    virtual void Unlock(const FString& AchievementId) = 0;
    virtual void Increment(const FString& AchievementId, int32 Step) = 0;
    virtual void ShowAchievements() = 0;
    virtual void SetToastEnable(bool bEnable) = 0;
    virtual FString GetPlatformName() const = 0;
};

/** 按 #if PLATFORM_* 创建并缓存单例平台实现。 */
class TAPACHIEVEMENT_API FTapAchievementPlatformFactory
{
public:
    static TSharedPtr<ITapAchievementPlatform> CreatePlatform();

    /** 释放缓存实例(模块卸载 / Editor live coding 时调用)。 */
    static void Reset();

private:
    static TSharedPtr<ITapAchievementPlatform> PlatformInstance;
};
