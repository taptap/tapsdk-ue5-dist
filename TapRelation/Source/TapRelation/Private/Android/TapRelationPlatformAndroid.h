// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if PLATFORM_ANDROID
#include "Platform/ITapRelationPlatform.h"

class FTapRelationPlatformAndroid : public ITapRelationPlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Prepare() override;
    virtual void StartMessenger() override;
    virtual void InviteGame() override;
    virtual void InviteTeam(const FString& TeamId) override;
    virtual void ShowTapUserProfile(const FString& OpenId, const FString& UnionId) override;
    virtual void GetNewFansCount(TFunction<void(int32)> Callback) override;
    virtual void GetUnreadMessageCount(TFunction<void(int32)> Callback) override;
    virtual void RegisterRelationCallback() override;
    virtual void UnregisterRelationCallback() override;
    virtual void RegisterRelationInviteCallback() override;
    virtual void UnregisterRelationInviteCallback() override;
    virtual void Destroy() override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }
};
#endif
