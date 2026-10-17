// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * HTTP 错误常量
 * 对齐 Unity 的 TapHttpErrorConstants
 */
class TAPCORE_API FTapHttpErrorConstants
{
public:
	// ============== 通用错误 ==============

	/** 时间校验失败 */
	static const FString ERROR_INVALID_TIME;

	/** 服务器内部错误 */
	static const FString ERROR_SERVER_ERROR;

	/** 无效请求 */
	static const FString ERROR_INVALID_REQUEST;

	/** 无效客户端 */
	static const FString ERROR_INVALID_CLIENT;

	/** 无效授权 */
	static const FString ERROR_INVALID_GRANT;

	/** 未授权的客户端 */
	static const FString ERROR_UNAUTHORIZED_CLIENT;

	/** 访问被拒绝 */
	static const FString ERROR_ACCESS_DENIED;

	/** 禁止访问 */
	static const FString ERROR_FORBIDDEN;

	/** 资源未找到 */
	static const FString ERROR_NOT_FOUND;

	/** 请求频率限制 */
	static const FString ERROR_RATE_LIMITED;

	// ============== 签名相关 ==============

	/** 无效签名 */
	static const FString ERROR_INVALID_SIGN;

	/** 签名已过期 */
	static const FString ERROR_SIGN_EXPIRED;

	/** 重放攻击 */
	static const FString ERROR_REPLAY_ATTACK;

	// ============== Token 相关 ==============

	/** Token 已过期 */
	static const FString ERROR_TOKEN_EXPIRED;

	/** Token 无效 */
	static const FString ERROR_TOKEN_INVALID;

	/** Token 已被撤销 */
	static const FString ERROR_TOKEN_REVOKED;

	// ============== 验证相关 ==============

	/** 需要验证码 */
	static const FString ERROR_CAPTCHA_REQUIRED;

	/** 验证码无效 */
	static const FString ERROR_CAPTCHA_INVALID;

	// ============== 防沉迷相关 ==============

	/** 未进行实名认证 */
	static const FString ERROR_ANTI_ADDICTION_NOT_VERIFIED;

	/** 防沉迷时间限制 */
	static const FString ERROR_ANTI_ADDICTION_TIME_LIMIT;

	/** 防沉迷充值限制 */
	static const FString ERROR_ANTI_ADDICTION_PAY_LIMIT;

	// ============== 用户相关 ==============

	/** 用户不存在 */
	static const FString ERROR_USER_NOT_FOUND;

	/** 用户已存在 */
	static const FString ERROR_USER_EXISTS;

	/** 用户已被禁用 */
	static const FString ERROR_USER_DISABLED;

	// ============== 参数相关 ==============

	/** 参数缺失 */
	static const FString ERROR_MISSING_PARAMETER;

	/** 参数格式错误 */
	static const FString ERROR_INVALID_PARAMETER;
};
