// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

/**
 * Rep Open 结果回调（对齐 Unity TapTapRep.Open 的 Action<int, string>）
 * Code: 0 表示成功，其它值为错误码；Msg: 错误信息（成功时为空）
 */
/** Rep 错误码（对齐 Unity TapRepError） */
namespace ETapRepError
{
    constexpr int32 PARAM_ERROR   = 100002; // 参数错误
    constexpr int32 CLIENT_ERROR  = 100003; // 客户端配置错误
    constexpr int32 UNKNOWN_ERROR = 100004; // 未知错误
    constexpr int32 NOT_INITIALIZED = 100005;
    constexpr int32 PLATFORM_NOT_AVAILABLE = 100006;
}
