// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * Desktop 合规模块常量集
 * 对齐 Android `com.taptap.sdk.compliance.constants.*` 与 Unity `ComplianceConst.cs`
 */
namespace TapComplianceConst
{
	// =========================================================
	// 启动 / 心跳 / 退出 等回调码 (StartUpResult)
	// 对齐 Unity StartUpResult / Android ComplianceMessage
	// =========================================================
	constexpr int32 LOGIN_SUCCESS                    = 500;   // 认证成功，可以进入游戏
	constexpr int32 EXITED                           = 1000;  // 用户退出 / Token 过期
	constexpr int32 SWITCH_ACCOUNT                   = 1001;  // 用户在限制弹窗中点击切换账号
	constexpr int32 PERIOD_RESTRICT                  = 1030;  // 时段 / 时长限制
	constexpr int32 DURATION_LIMIT                   = 1050;  // 保留：时长耗尽（当前未实际发出）
	constexpr int32 AGE_LIMIT                        = 1100;  // 内容分级不通过
	constexpr int32 INVALID_CLIENT_OR_NETWORK_ERROR  = 1200;  // 配置无效 / 网络错误
	constexpr int32 REAL_NAME_STOP                   = 9002;  // 实名流程被用户关闭

	// =========================================================
	// 服务端业务错误码
	// =========================================================
	constexpr int32 BUSINESS_TOKEN_EXPIRED           = 200000; // Token 失效
	constexpr int32 BUSINESS_IDENTITY_MISMATCH       = 200001; // 实名身份信息不一致

	// =========================================================
	// 年龄段
	// 对外: -1=未知, 0=<8 岁, 8=8 岁段, 16=16 岁段, 18=成年人
	// 内部哨兵: 100=未授权年龄的未成年, 110=未授权年龄的成年人 (永远不对外暴露)
	// =========================================================
	constexpr int32 AGE_LIMIT_UNKNOWN          = -1;
	constexpr int32 AGE_LIMIT_CHILD            = 0;
	constexpr int32 AGE_LIMIT_TEEN             = 8;
	constexpr int32 AGE_LIMIT_YOUNG            = 16;
	constexpr int32 AGE_LIMIT_ADULT            = 18;
	constexpr int32 AGE_LIMIT_SENTINEL_MINOR   = 100; // 未授权年龄的未成年
	constexpr int32 AGE_LIMIT_SENTINEL_ADULT   = 110; // 未授权年龄的成年人

	// =========================================================
	// 防沉迷策略
	// =========================================================
	const FString POLICY_ACTIVE_TIME_RANGE     = TEXT("time_range");
	const FString POLICY_MODEL_LOCAL           = TEXT("local");
	const FString POLICY_MODEL_SERVER          = TEXT("server");

	// =========================================================
	// 实名认证状态 (Verification.status)
	// =========================================================
	const FString VERIFICATION_STATUS_PASS     = TEXT("pass");
	const FString VERIFICATION_STATUS_WAITING  = TEXT("waiting");
	const FString VERIFICATION_STATUS_FAILED   = TEXT("failed");

	// =========================================================
	// 心跳默认值 (秒)
	// =========================================================
	constexpr int32 DEFAULT_HEARTBEAT_INTERVAL_SEC = 120;
	constexpr int32 ADULT_INFINITE_REMAIN_TIME    = 9999;

	// =========================================================
	// HTTP 端点路径
	// =========================================================
	const FString PATH_GLOBAL_CONFIG               = TEXT("/real-name/v1/get-global-config");
	const FString PATH_VERIFICATION                = TEXT("/real-name/v1/anti-addiction-token");
	const FString PATH_VERIFICATION_TAPTAP         = TEXT("/real-name/v1/anti-addiction-token-taptap");
	const FString PATH_VERIFICATION_MANUAL         = TEXT("/real-name/v1/anti-addiction-token-manual");
	const FString PATH_USER_ANTI_CONFIG            = TEXT("/anti-addiction/v1/get-config-by-token");
	const FString PATH_HEARTBEAT                   = TEXT("/anti-addiction/v1/heartbeat");
	const FString PATH_PAYABLE                     = TEXT("/anti-addiction/v1/payable");
	const FString PATH_SUBMIT_PAYMENT              = TEXT("/anti-addiction/v1/payment-submit");

	// =========================================================
	// HTTP Header
	// =========================================================
	const FString HEADER_ANTI_ADDICTION_TOKEN  = TEXT("X-TAP-Anti-Addiction-Token");
	const FString HEADER_AUTHORIZATION         = TEXT("Authorization");

	// =========================================================
	// 服务端业务错误标识
	// =========================================================
	const FString SERVER_ERROR_INVALID_TIME    = TEXT("invalid_time"); // MAC ts 与服务端时间相差过大

	// =========================================================
	// OAuth scope (用于 FastAuth)
	// =========================================================
	const FString SCOPE_COMPLIANCE             = TEXT("compliance");
	const FString SCOPE_COMPLIANCE_BASIC       = TEXT("compliance_basic");

	// =========================================================
	// 服务端 Host (China only — 合规模块仅大陆可用)
	// 默认指向生产；测试 / 调试可通过 SetHostOverride() 切换到研发 (xdrnd) 等环境
	// =========================================================
	const FString HOST_CN_PROD                     = TEXT("https://tapsdk.tapapis.cn");
	const FString HOST_CN_RND                      = TEXT("https://tapsdk.api.xdrnd.cn");

	/** 当前生效 Host：默认 HOST_CN_PROD；测试或调试可调用 SetHostOverride() 临时切换 */
	const FString& GetHostCN();

	/** 设置 Host 覆盖（传空串恢复默认）；进程级状态，不持久化 */
	void SetHostOverride(const FString& Host);

#if WITH_DEV_AUTOMATION_TESTS
	/** 测试辅助：读取显式 Host 覆盖；空串表示未覆盖。 */
	const FString& GetHostOverride();
#endif

	// =========================================================
	// 模块标识
	// =========================================================
	const FString MODULE_NAME                  = TEXT("TapCompliance");
	const FString MODULE_VERSION               = TEXT("1.0.0");
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
