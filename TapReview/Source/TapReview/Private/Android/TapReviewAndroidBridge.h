// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_ANDROID

/** TapReview Android 桥接（GameActivity helper routeToTapReview，对齐 TapUpdate） */
namespace TapReviewAndroidBridge
{
    /** 唤起评价界面（fire-and-forget，无回调） */
    void OpenReview();
}

#endif // PLATFORM_ANDROID
