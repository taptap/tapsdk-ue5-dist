// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID

/**
 * TapMoment Android 桥接
 * 路由：GameActivity.routeToTapMoment(json, callback)（单一方法，对齐 TapCloudSave + Unity 模式）
 * - fire-and-forget 命令（open/close 等）：callback = null
 * - setCallback：callback = EngineBridgeCallbackProxy（持久，通过 RegisterGlobalCallbackHandler 接收）
 * 回调 JSON：{"code":N,"msg":"..."} — 无额外 envelope
 */
namespace TapMomentAndroidBridge
{
    using FMomentCallback = TFunction<void(int32 /*Code*/, const FString& /*Msg*/)>;

    /** 注册 GlobalCallbackHandler（Init 只注册一次） */
    void Init(FMomentCallback OnCallback);

    void Open();
    void OpenScene(const FString& SceneId);
    void Publish(const FString& PublishMetaDataJson);
    void Close();
    void CloseWithTitle(const FString& Title, const FString& Content);
    void FetchNotification();
    void SetCallback();
}

#endif // PLATFORM_ANDROID
