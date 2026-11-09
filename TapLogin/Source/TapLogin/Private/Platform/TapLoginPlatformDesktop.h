// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Platform/ITapLoginPlatform.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Internal/Storage/TapMachineBoundPrefs.h"

#if PLATFORM_WINDOWS
#include "Desktop/TapLoginClientBridge.h"
#endif

struct FTapTokenData;
struct FTapHttpResponse;

/**
 * Desktop 平台登录实现（Windows/Mac/Linux）
 * 参照 Unity Standalone Login 实现
 * 显示 Slate 登录面板 (QR 码扫码 + 网页浏览器两种方式)
 */
class FTapLoginPlatformDesktop : public ITapLoginPlatform
{
public:
	FTapLoginPlatformDesktop();
	virtual ~FTapLoginPlatformDesktop() override;

	// ITapLoginPlatform implementation
	virtual void Initialize() override;
	virtual bool IsInitialized() const override;
	virtual void LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback) override;
	virtual void Logout() override;
	virtual TOptional<FTapAccount> GetCurrentAccount() const override;
	virtual FString GetPlatformName() const override;

	// FastAuth 支撑
	virtual bool HasScope(const FString& Scope) const override;
	virtual TArray<FString> GetCurrentScopes() const override;
	virtual FString BuildAuthorizationHeader(
		const FString& Method,
		const FString& Uri,
		const FString& Host,
		const FString& Port,
		int64          Timestamp) const override;
	virtual void AuthorizeForAccessToken(
		const TArray<ETapLoginScope>& Scopes,
		FTapAccessTokenCallback Callback) override;
	virtual void Authorize(
		const TArray<ETapLoginScope>& Scopes,
		TScriptInterface<ITapLoginCallback> Callback) override;

#if WITH_DEV_AUTOMATION_TESTS
	virtual void InjectTokenForTest(
		const FString& InKid,
		const FString& InAccessToken,
		const FString& InMacKey,
		const FString& InMacAlgorithm,
		const TArray<FString>& InScopes) override;
#endif

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FTapDesktopLoginTestAccess;
#endif
	bool bInitialized;
	mutable FCriticalSection AccountLock;
	TOptional<FTapAccount> CurrentAccount;
	TScriptInterface<ITapLoginCallback> PendingCallback;
	FTapAccessTokenCallback PendingAccessTokenCallback;
	bool bLoginInProgress = false;
	uint64 StateGeneration = 0;
	bool bCacheUserSameWithTapClient = true;
	bool bLauncherListenerRegistered = false;

	// 客户端授权配置
	FString ClientID;
	FString RedirectURI;

	// PKCE
	FString CodeVerifier;
	FString CodeChallenge;

	// CSRF State
	FString PendingState;

	// Token 信息 (MAC Token)
	FString TokenKid;
	FString TokenType;
	FString MacKey;
	FString MacAlgorithm;
	FString AccessToken;

	// Authorized scopes parsed from token responses. Requested scopes are not treated as granted scopes.
	TArray<FString> CurrentScopes;

	// 当前授权请求的 Scopes
	TArray<ETapLoginScope> PendingScopes;

	// 埋点用 Session ID 和登录方式
	FString LoginSessionId;
	FString CurrentLoginType;

	// Region URL
	FString GetWebHost() const;
	FString GetApiHost() const;

	// 权限转换
	FString ScopeToString(ETapLoginScope Scope) const;
	FString ScopesToParam(const TArray<ETapLoginScope>& Scopes) const;
	TArray<FString> ScopesToStringArray(const TArray<ETapLoginScope>& Scopes) const;
	FTapAccessToken MakeAccessToken(
		const FString& InKid,
		const FString& InTokenType,
		const FString& InMacKey,
		const FString& InMacAlgorithm,
		const TArray<FString>& InScopes) const;
	void CompleteAccessTokenAuthorizationSuccess(const FTapAccessToken& Token);
	void CompleteAccessTokenAuthorizationFailure(int32 ErrorCode, const FString& ErrorMessage);
	void CompleteAccessTokenAuthorizationCancelled();

	// PKCE + MAC 工具已提取到 FTapLoginCryptoUtils

	#if PLATFORM_WINDOWS
	bool StartNativeAuthorization();
	void HandleNativeAuthorizationComplete(bool bCancelled, const FString& RedirectUri);
	void HandleNativeAuthorizationFailure(const FString& Message);
	#endif

	// 启动器登录态同步与刷新
	void HandleLauncherCheckCompleted(const FString& OpenId);
	void ReconcileLauncherAccount(const FString& OpenId);
	void RefreshCachedToken();
	void HandleRefreshTokenResponse(uint64 RequestGeneration, const FTapHttpResponse& Result);

	// 面板回调
	void HandlePanelClose();
	void HandlePanelAuthSuccess(const FTapTokenData& TokenData, bool bWebLogin);

	void ExchangeCodeForToken(const FString& Code);
	void FetchUserInfo();
	void FetchUserInfoWithToken(const FString& InKid, const FString& InAccessToken,
		const FString& InMacKey, const FString& InMacAlgorithm, uint64 RequestGeneration = 0);

	// URL 解析
	static TMap<FString, FString> ParseQueryParams(const FString& QueryString);

	// 登录状态持久化
	FTapMachineBoundPrefs LoginPrefs;
	FTSTicker::FDelegateHandle LoginCacheTicker;
	static FString LoginCachePath(const FString& InClientId);
	void RestoreCachedLogin();
	bool TryRestoreCachedLogin(uint64 Generation);
	void BeginLogin();
	void SaveLoginState();
	void LoadLoginState();
	void ClearLoginState();

	// 清理客户端授权请求的临时状态
	void ClearNativeAuthorization();
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
