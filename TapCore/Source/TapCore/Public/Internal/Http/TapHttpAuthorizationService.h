// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

/**
 * MAC Authorization 提供者接口. 对齐:
 * - Android AuthorizationService.obtainAuthorization(url, method): String?
 * - Unity ITapLoginService.ObtainAuthorizationAsync(url, method)
 *
 * 设计要点:
 * 1. 只接 (Url, Method), 不接 timestamp. Provider 实现自己用
 *    FTapHttpTime::GetCurrentTime() (校准过的时钟) 拿时间戳.
 * 2. invalid_time 重试时 FTapHttp 重新调本方法, provider 自然拿到新时钟值,
 *    业务感知不到.
 *
 * TapCore 仅暴露接口; 实现由 TapLogin 提供 (FTapLoginAuthorizationServiceImpl),
 * 维持 TapCore 不反向依赖 TapLogin 的约束.
 */
class TAPCORE_API ITapHttpAuthorizationService
{
public:
    virtual ~ITapHttpAuthorizationService() = default;

    /**
     * @param Url    完整请求 URL (provider 内部自己拆 host:port:path)
     * @param Method HTTP method (GET/POST/...)
     * @return 完整 Authorization 头值 (如 "MAC id=...,ts=...,nonce=...,mac=...")
     *         未登录 / 缺凭据时返回空字符串
     */
    virtual FString ObtainAuthorization(const FString& Url, const FString& Method) = 0;
};
