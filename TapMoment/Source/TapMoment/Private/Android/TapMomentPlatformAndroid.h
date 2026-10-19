// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID
#include "Platform/ITapMomentPlatform.h"

class FTapMomentPlatformAndroid : public ITapMomentPlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Open() override;
    virtual void OpenScene(const FString& SceneId) override;
    virtual void Publish(const FTapMomentPostData& PostData) override;
    virtual void Close() override;
    virtual void CloseWithTitle(const FString& Title, const FString& Content) override;
    virtual void FetchNotification() override;
    virtual void SetCallback() override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }
};

#endif // PLATFORM_ANDROID
