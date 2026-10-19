// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID
#include "Platform/ITapSharePlatform.h"

class FTapSharePlatformAndroid : public ITapSharePlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Share(const FTapShareContent& Content) override;
    virtual void PickImage() override;
    virtual TArray<FString> GetPickedImagePaths() override;
    virtual void ClearPickedImages() override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }
};

#endif // PLATFORM_ANDROID
