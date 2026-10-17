// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceModels.h"

struct FTapAccessToken;

/**
 * Desktop 合规模块 HTTP 端点封装
 *
 * 网络底座: TapBasic FTapHttp Builder (HttpClient 单例, 构建于 Initialize())
 * 默认 Host: HOST_CN (合规模块仅大陆可用，区域门控在更上层完成)
 *
 * 错误归一化: 所有方法都把网络层异常 / 服务端业务错误统一塞进 FComplianceError 回传。
 *   - HTTP/local failures -> TransportErrorType + ErrorDescription
 *   - 服务端 success=false → ErrorCode/Error/ErrorDescription 解析自 data 节点
 *
 * 线程: Initialize / GetHttpClient 只允许在游戏线程调用；HTTP 请求异步，回调在游戏线程
 */
class FComplianceNetwork
{
public:
	/** 模块启动时调一次. 构建内部 HttpClient. */
	static void Initialize();

	/** 模块卸载时调一次. */
	static void Shutdown();

	using FOnVerification         = TFunction<void(bool /*bSuccess*/, const FComplianceVerification& /*Result*/, const FComplianceError& /*Err*/)>;
	using FOnRealNameConfig       = TFunction<void(bool, const FRealNameConfigResult&,    const FComplianceError&)>;
	using FOnUserAntiConfig       = TFunction<void(bool, const FUserAntiConfig&,          const FComplianceError&)>;
	using FOnPlayable             = TFunction<void(bool, const FPlayableResult&,          const FComplianceError&)>;
	using FOnPayable              = TFunction<void(bool, const FPayableResult&,           const FComplianceError&)>;
	using FOnVoid                 = TFunction<void(bool, const FComplianceError&)>;

	// =========================================================
	// 全局配置 (无 token)
	// GET /real-name/v1/get-global-config?client_id=&user_identifier=
	// =========================================================
	static void FetchGlobalConfig(const FString& UserId, FOnRealNameConfig OnDone);

	// =========================================================
	// 实名状态查询 (无 token)
	// GET /real-name/v1/anti-addiction-token?client_id=&user_identifier=
	// =========================================================
	static void FetchVerification(const FString& UserId, FOnVerification OnDone);

	// =========================================================
	// FastAuth: 用 TapToken (MAC 签名) 直接换实名结果
	// GET /real-name/v1/anti-addiction-token-taptap?client_id=&user_identifier=
	// bEnableAuthorization=true → AuthorizationService 全局注册自动 MAC + invalid_time 自动重签
	// =========================================================
	static void FetchVerificationByTapToken(const FString& UserId, FOnVerification OnDone);
	static void FetchVerificationByTapToken(
		const FString& UserId,
		const FTapAccessToken& Token,
		FOnVerification OnDone);

	// =========================================================
	// 手动实名 (无 token)
	// POST /real-name/v1/anti-addiction-token-manual?client_id=
	// body: {name, idcard, user_identifier}
	// =========================================================
	static void FetchVerificationManual(
		const FString& UserId,
		const FString& Name,
		const FString& IdCard,
		FOnVerification OnDone);

	// =========================================================
	// 用户防沉迷配置 (需 X-TAP-Anti-Addiction-Token)
	// GET /anti-addiction/v1/get-config-by-token?client_id=&user_identifier=&platform=pc
	// =========================================================
	static void FetchUserAntiConfig(const FString& UserId, FOnUserAntiConfig OnDone);

	// =========================================================
	// 心跳 (需 X-TAP-Anti-Addiction-Token)
	// POST /anti-addiction/v1/heartbeat?client_id=
	// body: {session_id, user_identifier}
	// =========================================================
	static void Heartbeat(
		const FString& UserId,
		const FString& SessionId,
		FOnPlayable OnDone);

	// =========================================================
	// 支付前校验 (需 X-TAP-Anti-Addiction-Token)
	// GET /anti-addiction/v1/payable?client_id=&user_identifier=&amount=
	// =========================================================
	static void CheckPayable(
		const FString& UserId,
		int64 AmountFen,
		FOnPayable OnDone);

	// =========================================================
	// 支付后上报 (需 X-TAP-Anti-Addiction-Token)
	// POST /anti-addiction/v1/payment-submit?client_id=
	// body: {amount, user_identifier}
	// =========================================================
	static void SubmitPayment(
		const FString& UserId,
		int64 AmountFen,
		FOnVoid OnDone);

#if WITH_DEV_AUTOMATION_TESTS
		/** 测试辅助：返回当前 HttpClient 捕获的 Host。 */
		static FString GetCurrentHostForTest();
#endif

private:
	/** FTapHttp 单例. Initialize() 时构建, Shutdown() 时释放. */
	static TSharedPtr<class FTapHttp> HttpClient;
	static FString HttpClientHost;

	/** 确保 HttpClient 存在，且 Host 跟随最新 TapConfig / HostOverride。 */
	static TSharedPtr<class FTapHttp> GetHttpClient();

	static FComplianceError MakeNetworkUnavailableError(const FString& Message);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
