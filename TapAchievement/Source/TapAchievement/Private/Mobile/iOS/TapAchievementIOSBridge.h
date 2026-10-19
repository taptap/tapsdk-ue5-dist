// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_IOS

/**
 * iOS 桥接：直接调用原生 BridgeAchievementService(ObjC 类方法)。
 * 与 TapCloudSaveIOSBridge 同构。registerCallback: 注册持久回调，
 * block 在原生回调队列触发，需 marshal 到 GameThread。
 * toast 由原生 SDK 自动弹出，UE 侧不处理 UI。
 */
namespace TapAchievementIOSBridge
{
    using FResultCallback = TFunction<void(const FString& /*ResultJson*/)>;

    /** 注册原生持久回调(调用一次)。 */
    void Init(FResultCallback OnPersistentResult);

    void Unlock(const FString& AchievementId);
    void Increment(const FString& AchievementId, int32 Steps);
    void ShowAchievements();
    void SetToastEnable(bool bEnable);
}

#endif // PLATFORM_IOS
