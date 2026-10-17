// Copyright TapTap. All Rights Reserved.
#include "TapReview.h"
#include "TapReviewModule.h"
#include "Internal/TapSdkChecker.h"

#if PLATFORM_ANDROID
#include "Android/TapReviewAndroidBridge.h"
#endif

void UTapReview::OpenReview()
{
    if (!FTapSdkChecker::CheckInitialize()) return;
#if PLATFORM_ANDROID
    TapReviewAndroidBridge::OpenReview();
#else
    UE_LOG(LogTapReview, Warning, TEXT("OpenReview is only supported on Android"));
#endif
}
