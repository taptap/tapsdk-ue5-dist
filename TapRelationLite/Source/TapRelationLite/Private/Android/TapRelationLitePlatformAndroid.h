// TapRelationLitePlatformAndroid.h
// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if PLATFORM_ANDROID
#include "Platform/ITapRelationLitePlatform.h"

class FTapRelationLitePlatformAndroid : public ITapRelationLitePlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void InviteGame() override;
    virtual void InviteTeam(const FString& TeamId) override;
    virtual void ShowTapUserProfile(const FString& OpenId, const FString& UnionId) override;
    virtual void GetFriendsList(const FString& Token, FRelationLiteListSuccess OnSuccess, FRelationLiteFailure OnFailure) override;
    virtual void GetFollowingList(const FString& Token, FRelationLiteListSuccess OnSuccess, FRelationLiteFailure OnFailure) override;
    virtual void GetFansList(const FString& Token, FRelationLiteListSuccess OnSuccess, FRelationLiteFailure OnFailure) override;
    virtual void SyncRelationshipWithOpenId(int32 A, const FString& N, const FString& FN, const FString& OpenId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF) override;
    virtual void SyncRelationshipWithUnionId(int32 A, const FString& N, const FString& FN, const FString& UnionId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF) override;
    virtual void RegisterRelationLiteCallback() override;
    virtual void UnregisterRelationLiteCallback() override;
    virtual void RegisterRelationLiteInviteCallback() override;
    virtual void UnregisterRelationLiteInviteCallback() override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }
};
#endif
