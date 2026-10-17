// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapHttpTypes.h"
#include "TapHttpResult.h"
#include "TapHttpRetryStrategy.h"
#include "TapBasicTypes.h"
#include "TapHttpSign.h"
#include "TapHttpParser.h"
#include "TapHttpAuthorizationService.h"

// 前向声明
class FTapHttpBuilder;

/**
 * HTTP 客户端配置
 */
struct TAPCORE_API FTapHttpConfig
{
	/** 模块名称 */
	FString ModuleName;

	/** 模块版本 */
	FString ModuleVersion;

	/** 自定义域名（为空则使用默认域名） */
	FString Domain;

	/** 连接超时（秒） */
	float ConnectTimeout = 10.0f;

	/** 读超时（秒） */
	float ReadTimeout = 5.0f;

	/** 写超时（秒） */
	float WriteTimeout = 5.0f;

	/** 是否启用签名 */
	bool bEnableSigning = true;

	/** 是否启用公共参数 */
	bool bEnableCommonParams = true;

	/** Sign 实现 (默认 FTapHttpSign::CreateDefaultSign) */
	TSharedPtr<ITapHttpSign> Sign;

	/** Parser 实现 (默认 FTapHttpParser::CreateEnvelopeParser) */
	TSharedPtr<ITapHttpParser> Parser;

	/** AuthorizationService 覆盖 (空则 fallback 到全局 FTapHttp::GetAuthorizationService) */
	TSharedPtr<ITapHttpAuthorizationService> AuthorizationService;
};

/**
 * 高级 HTTP 客户端
 * 对齐 Unity 的 TapHttp 类
 */
class TAPCORE_API FTapHttp : public TSharedFromThis<FTapHttp>
{
public:
	/** 域名常量 */
	static const FString HOST_CN;
	static const FString HOST_IO;
	static const FString HOST_RND_CN;
	static const FString HOST_RND_IO;

	/** 默认超时配置 */
	static constexpr float DEFAULT_CONNECT_TIMEOUT = 10.0f;
	static constexpr float DEFAULT_READ_TIMEOUT = 5.0f;
	static constexpr float DEFAULT_WRITE_TIMEOUT = 5.0f;
	static constexpr int32 DEFAULT_GET_RETRY_COUNT = 3;

	/**
	 * 初始化网络库
	 * 在 Core 初始化时调用，传入配置信息
	 * @param ClientID 客户端 ID
	 * @param ClientToken 客户端 Token
	 * @param Region 区域设置
	 * @param bEnableLog 是否启用日志
	 * @param bInIsRND 是否为 RND 环境
	 */
	static void Init(
		const FString& ClientID,
		const FString& ClientToken,
		ETapRegion Region,
		bool bEnableLog,
		bool bInIsRND = false
	);

	/**
	 * 全局注册 MAC Authorization 提供者. 对齐 Android ServiceManager.injectOrNull.
	 * TapLogin StartupModule 调一次, 所有 FTapHttp 实例的 bEnableAuthorization=true
	 * 都自动通过此 service 获取 Authorization 头.
	 */
	static void SetAuthorizationService(TSharedPtr<class ITapHttpAuthorizationService> Service);

	/**
	 * 获取已注册的全局 AuthorizationService (可能为空, 表示未注册).
	 */
	static TSharedPtr<class ITapHttpAuthorizationService> GetAuthorizationService();

	/**
	 * 检查网络库是否已初始化
	 * @return 是否已初始化
	 */
	static bool IsInitialized();

	/**
	 * 获取当前区域设置
	 * @return 当前区域
	 */
	static ETapRegion GetRegion();

	/**
	 * 获取是否启用日志
	 * @return 是否启用日志
	 */
	static bool IsLogEnabled();

	/**
	 * 创建 Builder
	 * @param ModuleName 模块名称
	 * @param ModuleVersion 模块版本
	 */
	static TSharedPtr<FTapHttpBuilder> NewBuilder(const FString& ModuleName, const FString& ModuleVersion);

	/**
	 * 构造函数（通过 Builder 调用）
	 */
	FTapHttp(const FTapHttpConfig& InConfig);

	/**
	 * GET 请求
	 * @param Path URL 路径
	 * @param QueryParams 查询参数
	 * @param bEnableAuthorization 是否启用授权
	 * @param RetryStrategy 重试策略（可选，默认 Fixed(3)）
	 * @param OnComplete 完成回调
	 */
	void Get(
		const FString& Path,
		const TMap<FString, FString>& QueryParams,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/**
	 * GET 请求（简化版本）
	 */
	void Get(
		const FString& Path,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/** GET (带 ExtraHeaders) — 业务自有头通过 ExtraHeaders 透传 */
	void Get(
		const FString& Path,
		const TMap<FString, FString>& QueryParams,
		const TMap<FString, FString>& ExtraHeaders,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/**
	 * POST JSON 请求
	 * @param Path URL 路径
	 * @param QueryParams 查询参数
	 * @param JsonBody JSON 请求体
	 * @param bEnableAuthorization 是否启用授权
	 * @param RetryStrategy 重试策略（可选，默认 None）
	 * @param OnComplete 完成回调
	 */
	void PostJson(
		const FString& Path,
		const TMap<FString, FString>& QueryParams,
		const TSharedPtr<FJsonObject>& JsonBody,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/**
	 * POST JSON 请求（简化版本）
	 */
	void PostJson(
		const FString& Path,
		const TSharedPtr<FJsonObject>& JsonBody,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/** POST JSON (带 ExtraHeaders) */
	void PostJson(
		const FString& Path,
		const TMap<FString, FString>& QueryParams,
		const TMap<FString, FString>& ExtraHeaders,
		const TSharedPtr<FJsonObject>& JsonBody,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/**
	 * POST Form 请求
	 * @param Path URL 路径
	 * @param QueryParams 查询参数
	 * @param FormData 表单数据
	 * @param bEnableAuthorization 是否启用授权
	 * @param RetryStrategy 重试策略（可选，默认 None）
	 * @param OnComplete 完成回调
	 */
	void PostForm(
		const FString& Path,
		const TMap<FString, FString>& QueryParams,
		const TMap<FString, FString>& FormData,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/**
	 * POST Form 请求（简化版本）
	 */
	void PostForm(
		const FString& Path,
		const TMap<FString, FString>& FormData,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/** POST Form (带 ExtraHeaders) */
	void PostForm(
		const FString& Path,
		const TMap<FString, FString>& QueryParams,
		const TMap<FString, FString>& ExtraHeaders,
		const TMap<FString, FString>& FormData,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete
	);

	/**
	 * 获取配置
	 */
	const FTapHttpConfig& GetConfig() const { return Config; }

private:
	FTapHttpConfig Config;

	/** 全局状态 - 是否已初始化 */
	static bool bIsInitialized;

	/** 全局状态 - 当前区域 */
	static ETapRegion CurrentRegion;

	/** 全局状态 - 是否启用日志 */
	static bool bEnableLog;

	/** 全局状态 - 是否为 RND 环境 */
	static bool bIsRND;

	/** 全局注册的 AuthorizationService. TapLogin 注册一次. */
	static TSharedPtr<class ITapHttpAuthorizationService> GlobalAuthorizationService;

	/** 获取 Host */
	FString GetHost() const;

	/** 构建完整 URL */
	FString BuildUrl(const FString& Path, const TMap<FString, FString>& QueryParams) const;

	/** 执行请求（带重试） */
	void ExecuteRequest(
		ETapHttpMethod Method,
		const FString& Url,
		const TMap<FString, FString>& Headers,
		const FString& Body,
		bool bEnableAuthorization,
		TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
		const FTapHttpJsonResultDelegate& OnComplete,
		int32 RetryCount = 0
	);


};

/**
 * HTTP 客户端 Builder
 * 对齐 Unity 的 TapHttpBuilder
 */
class TAPCORE_API FTapHttpBuilder : public TSharedFromThis<FTapHttpBuilder>
{
public:
	FTapHttpBuilder(const FString& ModuleName, const FString& ModuleVersion);

	/** 设置自定义域名 */
	TSharedPtr<FTapHttpBuilder> Domain(const FString& InDomain);

	/** 设置连接超时（秒） */
	TSharedPtr<FTapHttpBuilder> ConnectTimeout(float Seconds);

	/** 设置读超时（秒） */
	TSharedPtr<FTapHttpBuilder> ReadTimeout(float Seconds);

	/** 设置写超时（秒） */
	TSharedPtr<FTapHttpBuilder> WriteTimeout(float Seconds);

	/** 启用/禁用签名 */
	TSharedPtr<FTapHttpBuilder> EnableSigning(bool bEnable);

	/** 启用/禁用公共参数 */
	TSharedPtr<FTapHttpBuilder> EnableCommonParams(bool bEnable);

	/** 覆盖默认 Sign. */
	TSharedPtr<FTapHttpBuilder> Sign(TSharedRef<ITapHttpSign> InSign);

	/** 覆盖默认 Parser (EnvelopeParser). */
	TSharedPtr<FTapHttpBuilder> Parser(TSharedRef<ITapHttpParser> InParser);

	/** 覆盖全局 AuthorizationService (仅本 FTapHttp 实例生效). 主要用于测试. */
	TSharedPtr<FTapHttpBuilder> Authorization(TSharedRef<ITapHttpAuthorizationService> InService);

	/** 构建 HTTP 客户端 */
	TSharedPtr<FTapHttp> Build();

private:
	FTapHttpConfig Config;
};
