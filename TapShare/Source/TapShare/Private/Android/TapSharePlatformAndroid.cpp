// Copyright TapTap. All Rights Reserved.
#include "Android/TapSharePlatformAndroid.h"

#if PLATFORM_ANDROID

#include "Android/TapShareAndroidBridge.h"
#include "TapShare.h"
#include "TapShareModule.h"

void FTapSharePlatformAndroid::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    UE_LOG(LogTapShare, Log, TEXT("[TapShare-Android] initialized"));
}

void FTapSharePlatformAndroid::Share(const FTapShareContent& Content)
{
    const int32 Code = TapShareAndroidBridge::Share(Content);
    UTapShare::OnShareResult.Broadcast(Code);
}

void FTapSharePlatformAndroid::PickImage()
{
    TapShareAndroidBridge::PickImage();
}

TArray<FString> FTapSharePlatformAndroid::GetPickedImagePaths()
{
    return TapShareAndroidBridge::GetPickedImagePaths();
}

void FTapSharePlatformAndroid::ClearPickedImages()
{
    TapShareAndroidBridge::ClearPickedImages();
}

#endif // PLATFORM_ANDROID
