// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_ANDROID

/**
 * Android 桥接（路径 A：专属 routeToTapAchievement → TapAchievementUERouter）。
 *
 * 与 TapLogin / TapCompliance / TapLicense 同构：
 * - C++ 调 GameActivity.routeToTapAchievement(json, callback)
 * - 持久回调通过 JNI nativeOnAchievementEvent(String resultJson) 返回
 * - per-call 回调（暂无，预留）可通过 EngineBridgeCallbackProxy 扩展
 */
namespace TapAchievementAndroidBridge
{
    using FResultCallback = TFunction<void(const FString& /*ResultJson*/)>;

    /** 存储持久回调，在 Initialize 时调用一次 */
    void Init(FResultCallback OnPersistentResult);

    /** 向原生注册持久回调（发送 registerCallback 指令） */
    void RegisterCallback();

    void Unlock(const FString& AchievementId);
    void Increment(const FString& AchievementId, int32 Steps);
    void ShowAchievements();
    void SetToastEnable(bool bEnable);
}

#endif // PLATFORM_ANDROID
