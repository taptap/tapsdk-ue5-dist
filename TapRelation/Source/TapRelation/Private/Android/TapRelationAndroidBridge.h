// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID

/**
 * TapRelation Android 桥接
 * 路由：GameActivity.routeToTapRelation(json, callback)
 *   - fire-and-forget：callback=null
 *   - per-call（getNewFansCount 等）：one-shot EngineBridgeCallbackProxy
 *   - 持久（registerRelationCallback）：persistent proxy
 * CallbackId 命名空间：500000+（高于 TapMoment 400000）
 * 注意：relation_code 在 JSON 中是字符串 "N"，需 FCString::Atoi 转换
 */
namespace TapRelationAndroidBridge
{
    void Init();
    void Prepare();
    void StartMessenger();
    void InviteGame();
    void InviteTeam(const FString& TeamId);
    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);
    void GetNewFansCount(TFunction<void(int32)> Callback);
    void GetUnreadMessageCount(TFunction<void(int32)> Callback);
    void RegisterRelationCallback(TFunction<void(int32, int32, int32)> Callback);
    void UnregisterRelationCallback();
    void RegisterRelationInviteCallback(TFunction<void(const FString&, const FString&, const FString&)> Callback);
    void UnregisterRelationInviteCallback();
    void Destroy();
}

#endif // PLATFORM_ANDROID
