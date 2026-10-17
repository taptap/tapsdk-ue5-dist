// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"

/**
 * TapLogin 平台接口
 * 定义各平台需要实现的登录功能
 */
class TAPLOGIN_API ITapLoginPlatform
{
public:
	virtual ~ITapLoginPlatform() = default;

	/**
	 * 初始化平台特定的登录组件
	 */
	virtual void Initialize() = 0;

	/**
	 * 检查平台是否已初始化
	 */
	virtual bool IsInitialized() const = 0;

	/**
	 * 使用指定权限登录
	 * 对应 Android SDK: TapTapLogin.loginWithScopes()
	 * @param Scopes - 权限列表
	 * @param Callback - 登录回调
	 */
	virtual void LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback) = 0;

	/**
	 * 登出
	 * 对应 Android SDK: TapTapLogin.logout()
	 */
	virtual void Logout() = 0;

	/**
	 * 获取当前登录账号
	 * 对应 Android SDK: TapTapLogin.getCurrentTapAccount()
	 * @return 当前账号信息，如果未登录返回空
	 */
	virtual TOptional<FTapAccount> GetCurrentAccount() const = 0;

	// =========================================================
	// FastAuth (TapToken) 支撑接口
	//
	// Compliance / 其它需要复用 TapTap AccessToken 做 MAC 签名的模块通过这些接口取信息：
	//   - HasScope: 现有 token 是否包含特定 scope (用于判断能否复用)
	//   - GetCurrentScopes: 完整 scope 列表 (用于 UI 展示或日志)
	//   - BuildAuthorizationHeader: 用现有 token 的 kid + macKey 构造 "MAC ..." 头 (HMAC-SHA1/256 签名)
	//     调用方只需要传 method/uri/host/port/timestamp，secret 不离开 TapLogin
	//   - Authorize: 升级当前 token 的 scope（已有 → 直接回 cached；缺 → 触发 OAuth 走 LoginWithScopes）
	//
	// Mobile (Android/iOS) 端 compliance 走 native SDK，不需要这些 helper，stub 返回空即可。
	// =========================================================

	/** 当前 AccessToken 是否已授权指定 scope (大小写敏感) */
	virtual bool HasScope(const FString& Scope) const { return false; }

	/** 当前 AccessToken 已授权的 scope 列表 */
	virtual TArray<FString> GetCurrentScopes() const { return {}; }

	/**
	 * 构造 MAC 授权头（不含 "Authorization: " 前缀，含 "MAC " 前缀）
	 * 形如：MAC id="<kid>",ts="<ts>",nonce="<rand>",mac="<base64sig>"
	 *
	 * @param Method     HTTP 方法 (GET/POST/...)
	 * @param Uri        请求 URI 含 query (例：/real-name/v1/anti-addiction-token-taptap?client_id=xxx)
	 * @param Host       主机名 (例：tapsdk.tapapis.cn)
	 * @param Port       端口字符串 (例："443")
	 * @param Timestamp  Unix 秒；传 0 时由实现使用本地时间
	 * @return 已签名的 Authorization 头值；未登录或缺密钥时返回空字符串
	 */
	virtual FString BuildAuthorizationHeader(
		const FString& Method,
		const FString& Uri,
		const FString& Host,
		const FString& Port,
		int64          Timestamp) const { return FString(); }

	/**
	 * Token-only authorization. Desktop should return AccessToken fields without
	 * fetching profile or updating persisted login state.
	 */
	virtual void AuthorizeForAccessToken(
		const TArray<ETapLoginScope>& Scopes,
		FTapAccessTokenCallback Callback)
	{
		if (Callback)
		{
			FTapAccessTokenResult Result;
			Result.ErrorCode = -1;
			Result.ErrorMessage = TEXT("AuthorizeForAccessToken not supported");
			Callback(Result);
		}
	}

	/**
	 * 升级当前 token 的 scope。
	 *   - 已登录且 token 已包含全部 Scopes → 立刻回 OnLoginSuccess(当前完整账号)
	 *   - 否则 → 等价于触发一次 LoginWithScopes (Desktop 弹登录面板，Mobile 走 native)
	 *
	 * 与 LoginWithScopes 的区别：当 token 已满足 scope 时直接短路，不弹 UI。
	 */
	virtual void Authorize(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback)
	{
		// 默认实现：回退到全量登录
		LoginWithScopes(Scopes, Callback);
	}

	/**
	 * 获取平台名称（用于日志）
	 */
	virtual FString GetPlatformName() const = 0;

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * [仅测试] 直接注入 MAC token 字段，跳过真实 OAuth。
	 * 调用后 BuildAuthorizationHeader 即可基于注入值签名。
	 * 默认 no-op；需要支持 FastAuth 的平台（Desktop）应覆写。
	 */
	virtual void InjectTokenForTest(
		const FString& InKid,
		const FString& InAccessToken,
		const FString& InMacKey,
		const FString& InMacAlgorithm,
		const TArray<FString>& InScopes) {}
#endif
};

/**
 * TapLogin 平台工厂
 * 根据运行平台创建对应的实现
 */
class TAPLOGIN_API FTapLoginPlatformFactory
{
public:
	/**
	 * 创建当前平台的登录实现
	 * @return 平台实现实例
	 */
	static TSharedPtr<ITapLoginPlatform> CreatePlatform();

private:
	static TSharedPtr<ITapLoginPlatform> PlatformInstance;
};
