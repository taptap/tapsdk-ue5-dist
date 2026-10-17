// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID

/**
 * TapUpdate Android 桥接（GameActivity helper 路径，对齐 TapAchievement）
 *
 * UPL 在 GameActivity 注入：
 *   onCreate:         EngineBridge.init(this) + registerService(BridgeUpdateService, BridgeUpdateServiceImpl)
 *   routeToTapUpdate: EngineBridge.init(this) + execCommand(commandJson, null)
 *
 * C++ 通过 JNI 调 GameActivity.routeToTapUpdate(json)。
 * 回调通过 EngineBridge.nativeOnResult(String) 返回。
 */
namespace TapUpdateAndroidBridge
{
    /** 唤起更新 UI（用户取消时触发 nativeOnResult → OnUpdateCancel） */
    void UpdateGame(TFunction<void()> OnCancel);

    /** 触发强制更新检查（fire-and-forget） */
    void CheckForceUpdate();
}

#endif // PLATFORM_ANDROID
