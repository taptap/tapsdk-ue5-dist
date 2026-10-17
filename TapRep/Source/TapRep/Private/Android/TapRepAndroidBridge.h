// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID

/**
 * TapRep Android 桥接
 * 路由：GameActivity.routeToTapRep(json, callback)（对齐 TapMoment + Unity TapRepMobile）
 * - open：每次调用生成新 callbackId，传入 EngineBridgeCallbackProxy(id)，结果一次性回调
 * - 回调 JSON：EngineBridgeResult.toJson() = {"code":N,"message":..,"content":null}
 */
namespace TapRepAndroidBridge
{
    using FRepCallback = TFunction<void(int32 /*Code*/, const FString& /*Msg*/)>;

    /** 注册 GlobalCallbackHandler 与结果广播回调（Init 只注册一次） */
    void Init();

    /** 打开 URL（一次性回调） */
    void Open(const FString& OpenUrl, FRepCallback Callback);
}

#endif // PLATFORM_ANDROID
