// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 日志专用副本；不修改请求、响应或回调中的原始数据。 */
namespace TapLogPrivacy
{
	/** JSON / 表单按字段脱敏，普通文本及用户/设备标识保留。bPrivateData 用于 data 直接返回凭据的 Router。 */
	TAPCORE_API FString Body(const FString& Value, bool bPrivateData = false);
	/** 仅 OAuth 回调调用方将 bAuthorizationCode 设为 true，普通 URL 的 code 参数保留。 */
	TAPCORE_API FString Url(const FString& Value, bool bAuthorizationCode = false);
	TAPCORE_API FString Header(const FString& Key, const FString& Value);
}
