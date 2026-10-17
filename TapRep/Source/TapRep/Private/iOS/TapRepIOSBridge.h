// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_IOS

namespace TapRepIOSBridge
{
    using FRepCallback = TFunction<void(int32 /*Code*/, const FString& /*Msg*/)>;

    /** 注册结果广播回调（Init 时调用一次） */
    void Init();

    /** 打开 URL（completion 一次性回调，经 GameThread 广播） */
    void Open(const FString& OpenUrl, FRepCallback Callback);
}

#endif // PLATFORM_IOS
