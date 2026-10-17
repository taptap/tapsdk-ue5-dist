// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapShareTypes.h"

#if PLATFORM_IOS

namespace TapShareIOSBridge
{
    using FShareCallback = TFunction<void(int32 /*Code*/)>;

    /** 分享（completion 经 GameThread 回调结果码） */
    void Share(const FTapShareContent& Content, FShareCallback OnResult);
}

#endif // PLATFORM_IOS
