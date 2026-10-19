// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if PLATFORM_IOS
#include "TapRelationLiteTypes.h"

#ifndef TAP_RELATIONLITE_IOS_SDK_AVAILABLE
#define TAP_RELATIONLITE_IOS_SDK_AVAILABLE 0
#endif

namespace TapRelationLiteIOSBridge
{
    void RegisterOpenURLHandler();
    void UnregisterOpenURLHandler();
    using FListSuccess = TFunction<void(const FTapRelationLiteUserResult&)>;
    using FListFailure = TFunction<void(int32, const FString&)>;
    using FSyncSuccess = TFunction<void()>;
    using FSyncFailure = TFunction<void(const FString&)>;

    void Init(TFunction<void(int32)> OnPersistentResult);
    void Shutdown();
    void InviteGame();
    void InviteTeam(const FString& TeamId);
    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);
    void GetFriendsList(const FString& Token, FListSuccess OnSuccess, FListFailure OnFailure);
    void GetFollowingList(const FString& Token, FListSuccess OnSuccess, FListFailure OnFailure);
    void GetFansList(const FString& Token, FListSuccess OnSuccess, FListFailure OnFailure);
    void SyncRelationshipWithOpenId(int32 Action, const FString& N, const FString& FN, const FString& OpenId, FSyncSuccess OnSuccess, FSyncFailure OnFailure);
    void SyncRelationshipWithUnionId(int32 Action, const FString& N, const FString& FN, const FString& UnionId, FSyncSuccess OnSuccess, FSyncFailure OnFailure);
    void RegisterRelationLiteCallback();
    void UnregisterRelationLiteCallback();
    using FInviteCallback=TFunction<void(const FString&,const FString&,const FString&)>;
    void RegisterRelationLiteInviteCallback(FInviteCallback Callback);
    void UnregisterRelationLiteInviteCallback();
}
#endif
