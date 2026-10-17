// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapRelationTypes.h"

class TAPRELATION_API ITapRelationPlatform
{
public:
    virtual ~ITapRelationPlatform() = default;
    virtual void Initialize(const FTapTapSdkOptions& Config) = 0;
    virtual void Prepare() = 0;
    virtual void StartMessenger() = 0;
    virtual void InviteGame() = 0;
    virtual void InviteTeam(const FString& TeamId) = 0;
    virtual void ShowTapUserProfile(const FString& OpenId, const FString& UnionId) = 0;
    virtual void GetNewFansCount(TFunction<void(int32)> Callback) = 0;
    virtual void GetUnreadMessageCount(TFunction<void(int32)> Callback) = 0;
    virtual void RegisterRelationCallback() = 0;
    virtual void UnregisterRelationCallback() = 0;
    virtual void RegisterRelationInviteCallback() = 0;
    virtual void UnregisterRelationInviteCallback() = 0;
    virtual void Destroy() = 0;
    virtual FString GetPlatformName() const = 0;
};

class TAPRELATION_API FTapRelationPlatformFactory
{
public:
    static TSharedPtr<ITapRelationPlatform> CreatePlatform();
    static void Reset();
private:
    static TSharedPtr<ITapRelationPlatform> PlatformInstance;
};
