// Copyright TapTap. All Rights Reserved.
#include "TapUpdate.h"
#include "TapUpdateModule.h"
#include "Internal/TapSdkChecker.h"

#if PLATFORM_ANDROID
#include "Android/TapUpdateAndroidBridge.h"
#endif

void UTapUpdate::UpdateGame(const FTapUpdateCancelCallback& OnCancel)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
#if PLATFORM_ANDROID
    TapUpdateAndroidBridge::UpdateGame([OnCancel](){OnCancel.ExecuteIfBound();});
#else
    UE_LOG(LogTapUpdate, Warning, TEXT("UpdateGame is only supported on Android"));
#endif
}

void UTapUpdate::CheckForceUpdate()
{
    if (!FTapSdkChecker::CheckInitialize()) return;
#if PLATFORM_ANDROID
    TapUpdateAndroidBridge::CheckForceUpdate();
#else
    UE_LOG(LogTapUpdate, Warning, TEXT("CheckForceUpdate is only supported on Android"));
#endif
}
