// Copyright TapTap. All Rights Reserved.

/**
 * TapSDK HTTP 公共头文件
 *
 * 外部模块只需要 include 这一个文件即可使用所有 HTTP 功能：
 * #include "Internal/Http/TapHttpPublic.h"
 */

#pragma once

// 基础类型定义
#include "TapHttpTypes.h"

// 异常类体系
#include "TapHttpException.h"

// 结果包装类
#include "TapHttpResult.h"

// 时间校准
#include "TapHttpTime.h"

// 重试策略
#include "TapHttpRetryStrategy.h"

// 错误常量
#include "TapHttpErrorConstants.h"

// 高级 HTTP 客户端
#include "TapHttp.h"

// 基础 HTTP 请求（兼容旧代码）
#include "TapHttpRequest.h"

// 公共参数管理
#include "TapHttpCommonParams.h"
