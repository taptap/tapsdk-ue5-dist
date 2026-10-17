// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if PLATFORM_IOS

#ifndef TAP_RELATION_IOS_SDK_AVAILABLE
#define TAP_RELATION_IOS_SDK_AVAILABLE 0
#endif

namespace TapRelationIOSBridge
{
    void RegisterOpenURLHandler();
    void UnregisterOpenURLHandler();
    using FRelationCallback = TFunction<void(int32 /*Code*/, int32 /*Fans*/, int32 /*Unread*/)>;

    void Init(FRelationCallback OnPersistentResult);
    void Prepare();
    void StartMessenger();
    void InviteGame();
    void InviteTeam(const FString& TeamId);
    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);
    void GetNewFansCount(TFunction<void(int32)> Callback);
    void GetUnreadMessageCount(TFunction<void(int32)> Callback);
    void RegisterRelationCallback();
    void UnregisterRelationCallback();
    using FInviteCallback = TFunction<void(const FString&, const FString&, const FString&)>;
    void RegisterRelationInviteCallback(FInviteCallback Callback);
    void UnregisterRelationInviteCallback();
    void Destroy();
}
#endif
