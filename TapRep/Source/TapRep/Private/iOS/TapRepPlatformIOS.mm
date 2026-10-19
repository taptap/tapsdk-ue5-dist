// Copyright TapTap. All Rights Reserved.
#include "iOS/TapRepPlatformIOS.h"

#if PLATFORM_IOS

#include "iOS/TapRepIOSBridge.h"
#include "TapRepModule.h"

void FTapRepPlatformIOS::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    TapRepIOSBridge::Init();
}

void FTapRepPlatformIOS::Open(const FString& OpenUrl,TFunction<void(int32,const FString&)> Callback)
{
    TapRepIOSBridge::Open(OpenUrl,MoveTemp(Callback));
}

#endif // PLATFORM_IOS
