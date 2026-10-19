// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Dom/JsonObject.h"

/**
 * HTTP Request Result Structure (raw HTTP response)
 */
struct TAPCORE_API FTapHttpResponse
{
	/** Response success status */
	bool bSuccess = false;

	/** HTTP status code */
	int32 StatusCode = 0;

	/** Response body as string */
	FString Content;

	/** Response body as JSON object */
	TSharedPtr<FJsonObject> JsonObject;

	/** Error message */
	FString ErrorMessage;

	FTapHttpResponse() = default;

	/** Check if response is successful (2xx status code) */
	bool IsSuccess() const { return bSuccess && StatusCode >= 200 && StatusCode < 300; }
};

/**
 * HTTP Request Delegate
 */
DECLARE_DELEGATE_OneParam(FTapHttpRequestCompleteDelegate, const FTapHttpResponse&);

/**
 * 低层裸 HTTP API. 与高层 FTapHttp Builder 长期共存, 按场景选用.
 *
 * 使用判则:
 * - 端点是 OAuth 协议 (form-urlencoded body, OAuth response 而非 TapSDK envelope)
 *   → 用 FTapHttpRequest (本类). 例: TapLogin token exchange / QR device flow / profile.
 * - 端点是 TapSDK 业务 (envelope 响应, 需要 X-Tap-* 公共头, 可能需要 FastAuth MAC)
 *   → 用 FTapHttp::NewBuilder. 例: TapCompliance, 未来 TapWallet/Account.
 *
 * 详见: docs/superpowers/specs/2026-05-25-ue5-http-module-upgrade-design.md
 *   §4 使用判则
 *   §5 FTapHttpRequest → FTapHttp::NewBuilder 迁移指南
 *
 * 注意: 本类不是过渡品, 不会被 deprecated. TapCore.cpp 已为本类做全局配置
 *       (SetEnableCommonParams(false) + SetEnableSigning(false)), 专门给
 *       OAuth 路径用. 不要删 TapCore.cpp:154-156 这 3 行配置.
 */
class TAPCORE_API FTapHttpRequest
{
public:
	/**
	 * Create a GET request
	 * @param URL The request URL
	 * @param OnComplete Callback when request completes
	 * @param Timeout Request timeout in seconds (default: 30)
	 */
	static void Get(
		const FString& URL,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout = 30.0f
	);

	/**
	 * Create a POST request with JSON body
	 * @param URL The request URL
	 * @param JsonBody JSON body as string
	 * @param OnComplete Callback when request completes
	 * @param Timeout Request timeout in seconds (default: 30)
	 */
	static void Post(
		const FString& URL,
		const FString& JsonBody,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout = 30.0f
	);

	/**
	 * Create a POST request with JSON object
	 * @param URL The request URL
	 * @param JsonObject JSON body as object
	 * @param OnComplete Callback when request completes
	 * @param Timeout Request timeout in seconds (default: 30)
	 */
	static void PostJson(
		const FString& URL,
		const TSharedPtr<FJsonObject>& JsonObject,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout = 30.0f
	);

	/**
	 * Create a PUT request with JSON body
	 * @param URL The request URL
	 * @param JsonBody JSON body as string
	 * @param OnComplete Callback when request completes
	 * @param Timeout Request timeout in seconds (default: 30)
	 */
	static void Put(
		const FString& URL,
		const FString& JsonBody,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout = 30.0f
	);

	/**
	 * Create a DELETE request
	 * @param URL The request URL
	 * @param OnComplete Callback when request completes
	 * @param Timeout Request timeout in seconds (default: 30)
	 */
	static void Delete(
		const FString& URL,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout = 30.0f
	);

	/**
	 * Create a custom HTTP request with headers
	 * @param Method HTTP method (GET, POST, PUT, DELETE, etc.)
	 * @param URL The request URL
	 * @param Headers Custom headers
	 * @param Body Request body (optional)
	 * @param OnComplete Callback when request completes
	 * @param Timeout Request timeout in seconds (default: 30)
	 */
	static void Request(
		const FString& Method,
		const FString& URL,
		const TMap<FString, FString>& Headers,
		const FString& Body,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout = 30.0f
	);

	/**
	 * 发送一个带 TapSDK 签名的请求, 每次 attempt 都从 FTapHttpTime 重新渲染 X-Tap-Ts 并重签 X-Tap-Sign.
	 *
	 * 对齐 Android TapHttpCall + TapHttpRetry + TapHttpSign:
	 *   - 出站: 每个 attempt 调用 FTapHttpCommonParams::GetCommonHeaders / AddSignature, X-Tap-Ts 取自
	 *     FTapHttpTime::GetCurrentTime() (FixTime 校准后会自动用校准值).
	 *   - 入站: 响应里的 envelope.now 会调 FTapHttpTime::FixTime 校准时钟. 如果 HTTP 400 且
	 *     envelope.error == "invalid_time" 且 bEnableInvalidTimeRetry, 自动重发一次 (重新构建
	 *     header + 重签), 业务回调感知不到.
	 *
	 * 设计要求: 调用方不要再自己塞 X-Tap-* / X-Tap-Sign, 否则 attempt 2 重发的会是陈旧值.
	 *
	 * @param Method                    HTTP method
	 * @param URL                       完整 URL (含 query)
	 * @param ExtraHeaders              调用方自己的 header (Content-Type, Authorization, etc.)
	 *                                  公共头 (X-Tap-*) 和 X-Tap-Sign 不要在这里塞, Request 层会加.
	 * @param Body                      请求体
	 * @param ModuleName                调用 GetCommonHeaders 用的模块名, e.g. "TapCompliance"
	 * @param ModuleVersion             调用 GetCommonHeaders 用的模块版本
	 * @param bEnableAuthorization      传给 GetCommonHeaders, 决定是否塞 Authorization 头
	 * @param OnComplete                完成回调
	 * @param bEnableInvalidTimeRetry   true: 收到 invalid_time 自动重发一次. 调用方有自己的
	 *                                  invalid_time 重试 (如 FastAuth 的 MAC 重签) 时传 false 避免重复
	 * @param Timeout                   请求超时 (秒)
	 */
	static void RequestWithSigning(
		const FString& Method,
		const FString& URL,
		const TMap<FString, FString>& ExtraHeaders,
		const FString& Body,
		const FString& ModuleName,
		const FString& ModuleVersion,
		bool bEnableAuthorization,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		bool bEnableInvalidTimeRetry = true,
		float Timeout = 30.0f
	);

	/**
	 * Set default headers for all requests
	 * @param Headers Default headers
	 */
	static void SetDefaultHeaders(const TMap<FString, FString>& Headers);

	/**
	 * Add a default header
	 * @param Key Header key
	 * @param Value Header value
	 */
	static void AddDefaultHeader(const FString& Key, const FString& Value);

	/**
	 * Clear all default headers
	 */
	static void ClearDefaultHeaders();

	/**
	 * Enable or disable common parameters (headers and query params)
	 * @param bEnable true to enable, false to disable
	 */
	static void SetEnableCommonParams(bool bEnable);

	/**
	 * Set module information for common parameters
	 * @param ModuleName Module name (e.g., "TapCore", "TapLogin")
	 * @param ModuleVersion Module version (e.g., "1.0.0")
	 */
	static void SetModuleInfo(const FString& ModuleName, const FString& ModuleVersion);

	/**
	 * Enable or disable request signing
	 * @param bEnable true to enable signing, false to disable
	 */
	static void SetEnableSigning(bool bEnable);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTapHttpRequestVersionTest;
#endif

	/** Default headers applied to all requests */
	static TMap<FString, FString> DefaultHeaders;

	/** Whether to enable common parameters */
	static bool bEnableCommonParams;

	/** Whether to enable request signing */
	static bool bEnableSigning;

	/** Module name for common parameters */
	static FString ModuleName;

	/** Module version for common parameters */
	static FString ModuleVersion;

	/**
	 * Process HTTP response
	 * @param Request The HTTP request
	 * @param Response The HTTP response
	 * @param bWasSuccessful Whether the request was successful
	 * @param OnComplete Callback delegate
	 */
	static void ProcessResponse(
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bWasSuccessful,
		const FTapHttpRequestCompleteDelegate& OnComplete
	);

	/**
	 * Create and configure HTTP request
	 * @param Method HTTP method
	 * @param URL Request URL
	 * @param Timeout Request timeout
	 * @return Configured HTTP request
	 */
	static TSharedRef<IHttpRequest> CreateRequest(
		const FString& Method,
		const FString& URL,
		float Timeout
	);
};
