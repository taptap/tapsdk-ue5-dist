// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_IOS
#include "Platform/ITapSharePlatform.h"

class FTapSharePlatformIOS : public ITapSharePlatform
{
public:
    virtual void Initialize(const FTapTapSdkOptions& Config) override;
    virtual void Share(const FTapShareContent& Content) override;
    virtual FString GetPlatformName() const override { return TEXT("iOS"); }
};

#endif // PLATFORM_IOS
