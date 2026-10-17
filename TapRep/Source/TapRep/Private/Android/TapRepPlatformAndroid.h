// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID
#include "Platform/ITapRepPlatform.h"

class FTapRepPlatformAndroid : public ITapRepPlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Open(const FString& OpenUrl,TFunction<void(int32,const FString&)> Callback) override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }
};

#endif // PLATFORM_ANDROID
