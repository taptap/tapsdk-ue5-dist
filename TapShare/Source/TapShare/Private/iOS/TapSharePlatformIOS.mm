// Copyright TapTap. All Rights Reserved.
#include "iOS/TapSharePlatformIOS.h"

#if PLATFORM_IOS

#include "iOS/TapShareIOSBridge.h"
#include "TapShare.h"
#include "TapShareModule.h"

void FTapSharePlatformIOS::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    UE_LOG(LogTapShare, Log, TEXT("[TapShare-iOS] initialized"));
}

void FTapSharePlatformIOS::Share(const FTapShareContent& Content)
{
    TapShareIOSBridge::Share(Content, [](int32 Code)
    {
        UTapShare::OnShareResult.Broadcast(Code);
    });
}

#endif // PLATFORM_IOS
