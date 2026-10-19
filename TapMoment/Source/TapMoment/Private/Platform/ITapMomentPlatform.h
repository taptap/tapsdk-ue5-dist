// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapMomentTypes.h"

class TAPMOMENT_API ITapMomentPlatform
{
public:
    virtual ~ITapMomentPlatform() = default;

    virtual void Initialize(const FTapTapSdkOptions& Config) = 0;
    virtual void Open() = 0;
    virtual void OpenScene(const FString& SceneId) = 0;
    virtual void Publish(const FTapMomentPostData& PostData) = 0;
    virtual void Close() = 0;
    virtual void CloseWithTitle(const FString& Title, const FString& Content) = 0;
    virtual void FetchNotification() = 0;
    virtual void SetCallback() = 0;
    virtual FString GetPlatformName() const = 0;
};

class TAPMOMENT_API FTapMomentPlatformFactory
{
public:
    static TSharedPtr<ITapMomentPlatform> CreatePlatform();
    static void Reset();
private:
    static TSharedPtr<ITapMomentPlatform> PlatformInstance;
};
