// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"

class TAPREP_API ITapRepPlatform
{
public:
    virtual ~ITapRepPlatform() = default;

    virtual void Initialize(const FTapTapSdkOptions& Config) = 0;
    virtual void Open(const FString& OpenUrl, TFunction<void(int32,const FString&)> Callback) = 0;
    virtual FString GetPlatformName() const = 0;
};

class TAPREP_API FTapRepPlatformFactory
{
public:
    static TSharedPtr<ITapRepPlatform> CreatePlatform();
    static void Reset();
private:
    static TSharedPtr<ITapRepPlatform> PlatformInstance;
};
