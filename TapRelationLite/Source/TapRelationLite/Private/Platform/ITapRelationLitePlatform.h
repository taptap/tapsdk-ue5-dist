// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapRelationLiteTypes.h"

using FRelationLiteListSuccess = TFunction<void(const FTapRelationLiteUserResult&)>;
using FRelationLiteFailure     = TFunction<void(int32, const FString&)>;
using FRelationLiteSyncSuccess = TFunction<void()>;
using FRelationLiteSyncFailure = TFunction<void(const FString&)>;

class TAPRELATIONLITE_API ITapRelationLitePlatform
{
public:
    virtual ~ITapRelationLitePlatform() = default;
    virtual void Initialize(const FTapTapSdkOptions& Config) = 0;
    virtual void InviteGame() = 0;
    virtual void InviteTeam(const FString& TeamId) = 0;
    virtual void ShowTapUserProfile(const FString& OpenId, const FString& UnionId) = 0;
    virtual void GetFriendsList(const FString& NextPageToken, FRelationLiteListSuccess OnSuccess, FRelationLiteFailure OnFailure) = 0;
    virtual void GetFollowingList(const FString& NextPageToken, FRelationLiteListSuccess OnSuccess, FRelationLiteFailure OnFailure) = 0;
    virtual void GetFansList(const FString& NextPageToken, FRelationLiteListSuccess OnSuccess, FRelationLiteFailure OnFailure) = 0;
    virtual void SyncRelationshipWithOpenId(int32 Action, const FString& Nickname, const FString& FriendNickname, const FString& FriendOpenId, FRelationLiteSyncSuccess OnSuccess, FRelationLiteSyncFailure OnFailure) = 0;
    virtual void SyncRelationshipWithUnionId(int32 Action, const FString& Nickname, const FString& FriendNickname, const FString& FriendUnionId, FRelationLiteSyncSuccess OnSuccess, FRelationLiteSyncFailure OnFailure) = 0;
    virtual void RegisterRelationLiteCallback() = 0;
    virtual void UnregisterRelationLiteCallback() = 0;
    virtual void RegisterRelationLiteInviteCallback() = 0;
    virtual void UnregisterRelationLiteInviteCallback() = 0;
    virtual FString GetPlatformName() const = 0;
};

class TAPRELATIONLITE_API FTapRelationLitePlatformFactory
{
public:
    static TSharedPtr<ITapRelationLitePlatform> CreatePlatform();
    static void Reset();
private:
    static TSharedPtr<ITapRelationLitePlatform> PlatformInstance;
};
