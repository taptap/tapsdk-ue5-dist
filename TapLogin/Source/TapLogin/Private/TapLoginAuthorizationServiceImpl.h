// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Http/TapHttpAuthorizationService.h"

/**
 * TapLogin 提供的 ITapHttpAuthorizationService 实现.
 * 委托给 FTapTapLogin::BuildAuthorizationHeader 计算 MAC 头.
 * 时间戳从 FTapHttpTime::GetCurrentTime() 拿 (校准过), 不接受外部 timestamp 参数.
 */
class TAPLOGIN_API FTapLoginAuthorizationServiceImpl : public ITapHttpAuthorizationService
{
public:
	virtual FString ObtainAuthorization(const FString& Url, const FString& Method) override;
};
