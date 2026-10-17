// Copyright TapTap. All Rights Reserved.
#include "Android/TapRepPlatformAndroid.h"

#if PLATFORM_ANDROID

#include "Android/TapRepAndroidBridge.h"
#include "TapRepModule.h"

void FTapRepPlatformAndroid::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    TapRepAndroidBridge::Init();
}

void FTapRepPlatformAndroid::Open(const FString& OpenUrl,TFunction<void(int32,const FString&)> Callback)
{
    TapRepAndroidBridge::Open(OpenUrl,MoveTemp(Callback));
}

#endif // PLATFORM_ANDROID
