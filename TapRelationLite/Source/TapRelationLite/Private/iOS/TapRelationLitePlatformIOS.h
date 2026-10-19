// TapRelationLitePlatformIOS.h
// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if PLATFORM_IOS
#include "Platform/ITapRelationLitePlatform.h"

class FTapRelationLitePlatformIOS : public ITapRelationLitePlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void InviteGame() override;
    virtual void InviteTeam(const FString& TeamId) override;
    virtual void ShowTapUserProfile(const FString& OpenId, const FString& UnionId) override;
    virtual void GetFriendsList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF) override;
    virtual void GetFollowingList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF) override;
    virtual void GetFansList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF) override;
    virtual void SyncRelationshipWithOpenId(int32 A, const FString& N, const FString& FN, const FString& OId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF) override;
    virtual void SyncRelationshipWithUnionId(int32 A, const FString& N, const FString& FN, const FString& UId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF) override;
    virtual void RegisterRelationLiteCallback() override;
    virtual void UnregisterRelationLiteCallback() override;
    virtual void RegisterRelationLiteInviteCallback() override;
    virtual void UnregisterRelationLiteInviteCallback() override;
    virtual FString GetPlatformName() const override { return TEXT("iOS"); }
};
#endif
