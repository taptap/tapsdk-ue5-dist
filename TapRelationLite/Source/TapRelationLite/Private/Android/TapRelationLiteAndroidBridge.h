// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID
#include "TapRelationLiteTypes.h"

/**
 * TapRelationLite Android 桥接
 * CallbackId 命名空间：600000+
 * 注意：
 *   - friends_list/following_list/fans_list 是二次序列化字符串，需两次反序列化
 *   - relation_lite_result_code 是字符串 "N"
 */
namespace TapRelationLiteAndroidBridge
{
    using FListSuccess = TFunction<void(const FTapRelationLiteUserResult&)>;
    using FListFailure = TFunction<void(int32, const FString&)>;
    using FSyncSuccess = TFunction<void()>;
    using FSyncFailure = TFunction<void(const FString&)>;

    void Init();
    void InviteGame();
    void InviteTeam(const FString& TeamId);
    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);
    void GetFriendsList(const FString& NextPageToken, FListSuccess OnSuccess, FListFailure OnFailure);
    void GetFollowingList(const FString& NextPageToken, FListSuccess OnSuccess, FListFailure OnFailure);
    void GetFansList(const FString& NextPageToken, FListSuccess OnSuccess, FListFailure OnFailure);
    void SyncRelationshipWithOpenId(int32 Action, const FString& Nickname, const FString& FriendNickname, const FString& FriendOpenId, FSyncSuccess OnSuccess, FSyncFailure OnFailure);
    void SyncRelationshipWithUnionId(int32 Action, const FString& Nickname, const FString& FriendNickname, const FString& FriendUnionId, FSyncSuccess OnSuccess, FSyncFailure OnFailure);
    void RegisterRelationLiteCallback(TFunction<void(int32)> Callback);
    void UnregisterRelationLiteCallback();
    using FInviteCallback=TFunction<void(const FString&,const FString&,const FString&)>;
    void RegisterRelationLiteInviteCallback(FInviteCallback Callback);
    void UnregisterRelationLiteInviteCallback();
}
#endif
