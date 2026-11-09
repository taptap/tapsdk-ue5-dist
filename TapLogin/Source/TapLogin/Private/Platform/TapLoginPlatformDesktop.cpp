// Copyright TapTap. All Rights Reserved.

#include "TapLoginPlatformDesktop.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "Internal/TapComplianceScopeProvider.h"
#include "TapLoginCryptoUtils.h"
#include "TapLoginPanelManager.h"
#include "TapQRCodeService.h"
#include "TapLoginModule.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/Platform/TapOpenlog.h"
#include "Internal/Platform/TapAppDuration.h"
#include "Platform/TapLoginTracker.h"
#include "Internal/Http/TapHttpRequest.h"
#include "Internal/Http/TapHttpErrorConstants.h"
#include "Internal/Http/TapHttpTime.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#if PLATFORM_WINDOWS
#include "Desktop/TapLoginClientBridge.h"
#include "Internal/Storage/TapDeviceId.h"
#include "Launcher/TapLauncher.h"
#include "Internal/TapLauncherInternal.h"
#endif

namespace
{
	void AppendScopeIfMissing(TArray<ETapLoginScope>& Scopes, ETapLoginScope Scope)
	{
		if (!Scopes.Contains(Scope))
		{
			Scopes.Add(Scope);
		}
	}

	TArray<ETapLoginScope> BuildDesktopLoginScopes(const TArray<ETapLoginScope>& Scopes)
	{
		TArray<ETapLoginScope> Result = Scopes;
		AppendScopeIfMissing(Result, ETapLoginScope::PublicProfile);

		const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
		if (Config.Region == ETapRegion::CN && !Config.ClientID.IsEmpty())
		{
			if (const TOptional<ETapLoginScope> ComplianceScope = FTapComplianceScopeProvider::Resolve();
				ComplianceScope.IsSet())
			{
				AppendScopeIfMissing(Result, ComplianceScope.GetValue());
			}
		}
		return Result;
	}
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapLoginPlatformDesktop::FTapLoginPlatformDesktop()
	: bInitialized(false)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] FTapLoginPlatformDesktop created"));
}

FTapLoginPlatformDesktop::~FTapLoginPlatformDesktop()
{
	FTSTicker::GetCoreTicker().RemoveTicker(LoginCacheTicker);
	// 析构时日志系统可能已被销毁（如点击窗口 X 退出时），不能调用 UE_LOG
#if PLATFORM_WINDOWS
	FTapLauncherInternal::OnLauncherCheckCompleted.RemoveAll(this);
	FTapLoginClientBridge::Shutdown();
#endif
}

// =============================================================================
// Region Endpoints (参照 Unity Region.cs)
// =============================================================================

FString FTapLoginPlatformDesktop::GetWebHost() const
{
	const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
	if (FTapSdkConfigState::IsRNDEnvironment())
	{
		return (Config.Region == ETapRegion::Global)
			? TEXT("https://oauth.api.xdrnd.com")
			: TEXT("https://oauth.api.xdrnd.cn");
	}
	return (Config.Region == ETapRegion::Global)
		? TEXT("https://accounts.tapapis.com")
		: TEXT("https://accounts.tapapis.cn");
}

FString FTapLoginPlatformDesktop::GetApiHost() const
{
	const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
	if (FTapSdkConfigState::IsRNDEnvironment())
	{
		return (Config.Region == ETapRegion::Global)
			? TEXT("https://open.api.xdrnd.com")
			: TEXT("https://open.api.xdrnd.cn");
	}
	return (Config.Region == ETapRegion::Global)
		? TEXT("https://open.tapapis.com")
		: TEXT("https://open.tapapis.cn");
}

// =============================================================================
// ITapLoginPlatform Implementation
// =============================================================================

void FTapLoginPlatformDesktop::Initialize()
{
	if (bInitialized)
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Desktop] TapLogin already initialized"));
		return;
	}

	const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
	ClientID = Config.ClientID;
	if (ClientID.IsEmpty())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Desktop] ClientID is empty, TapLogin cannot initialize"));
		return;
	}

	bInitialized = true;

	// 初始化持久化存储并恢复登录状态
	// Legacy account.json has no client ownership metadata and cannot be safely imported.
	LoginPrefs.Init(LoginCachePath(ClientID));
#if PLATFORM_WINDOWS
	FTapLauncherInternal::OnLauncherCheckCompleted.AddRaw(this, &FTapLoginPlatformDesktop::HandleLauncherCheckCompleted);
#endif
	if (LoginPrefs.EnsureReady()) RestoreCachedLogin();
	else
	{
		const uint64 Generation = StateGeneration;
		LoginCacheTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, Generation](float)
		{
			if (!TryRestoreCachedLogin(Generation)) return true;
			LoginCacheTicker.Reset();
			return false;
		}), 1.0f);
	}

	FTapLoginTracker::Get().TrackInit();

	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] TapLogin initialized - ClientID: %s, Platform: %s"),
		*ClientID, *GetPlatformName());
}

bool FTapLoginPlatformDesktop::IsInitialized() const
{
	return bInitialized;
}

FString FTapLoginPlatformDesktop::LoginCachePath(const FString& InClientId)
{
	return FPaths::Combine(TEXT("TapLogin"), FMD5::HashAnsiString(*InClientId), TEXT("account.json"));
}

void FTapLoginPlatformDesktop::RestoreCachedLogin()
{
	LoadLoginState();
#if PLATFORM_WINDOWS
	FString LauncherOpenId;
	if (FTapLauncherInternal::GetCachedOpenId(LauncherOpenId)) HandleLauncherCheckCompleted(LauncherOpenId);
#endif
	RefreshCachedToken();
}

bool FTapLoginPlatformDesktop::TryRestoreCachedLogin(uint64 Generation)
{
	// Never restore a delayed startup snapshot over a new login or logout.
	{
		FScopeLock Lock(&AccountLock);
		if (Generation != StateGeneration || bLoginInProgress || CurrentAccount.IsSet()) return true;
	}
	if (!LoginPrefs.EnsureReady()) return false;
	RestoreCachedLogin();
	return true;
}

void FTapLoginPlatformDesktop::BeginLogin()
{
	FScopeLock Lock(&AccountLock);
	++StateGeneration; // Retire startup restoration even if this login finishes before its next tick.
	bLoginInProgress = true;
}

void FTapLoginPlatformDesktop::LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] LoginWithScopes - Scopes count: %d"), Scopes.Num());

	if (!bInitialized)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Desktop] TapLogin not initialized"));
		if (Callback.GetInterface())
		{
			Callback->OnLoginFailed(-1, TEXT("TapLogin not initialized"));
		}
		return;
	}

	if (!Callback.GetInterface())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Desktop] Callback is null"));
		return;
	}

	if (bLoginInProgress || PendingAccessTokenCallback)
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Desktop] Login already in progress"));
		Callback->OnLoginFailed(-1, TEXT("Currently logging in"));
		return;
	}

	BeginLogin();
	PendingCallback = Callback;
	PendingAccessTokenCallback = FTapAccessTokenCallback();
	PendingScopes = BuildDesktopLoginScopes(Scopes);

	// 生成埋点 SessionId 并上报 start 事件
	LoginSessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	CurrentLoginType.Empty();
	FTapLoginTracker::Get().TrackStart(TEXT("loginWithScopes"), LoginSessionId);

	// 转换 Scopes 为字符串数组 (面板需要)
	TArray<FString> ScopeStringArray = ScopesToStringArray(PendingScopes);

#if PLATFORM_WINDOWS
	if (UTapLauncher::IsPassedLauncherCheck() && FTapLoginClientBridge::IsAvailable())
	{
		if (StartNativeAuthorization())
		{
			UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Tap PC native authorization started"));
			return;
		}
		HandleNativeAuthorizationFailure(TEXT("Tap PC native authorization failed to start"));
		return;
	}
#endif

	// 显示登录面板 (QR 码 + 网页两种方式)
	FTapLoginPanelManager::Get().ShowPanel(
		ClientID,
		ScopeStringArray,
		FOnLoginPanelClose::CreateRaw(this, &FTapLoginPlatformDesktop::HandlePanelClose),
		FOnLoginPanelAuthSuccess::CreateRaw(this, &FTapLoginPlatformDesktop::HandlePanelAuthSuccess)
	);

	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Login panel shown"));
}

void FTapLoginPlatformDesktop::Logout()
{
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Logout"));
#if PLATFORM_WINDOWS
	FTapLoginClientBridge::Cancel();
#endif
	{
		FScopeLock Lock(&AccountLock);
		++StateGeneration;
		bCacheUserSameWithTapClient = true;
	}
	FTapLoginPanelManager::Get().ClosePanel();
	{
		FScopeLock Lock(&AccountLock);
		CurrentAccount.Reset();
		AccessToken.Empty();
		TokenKid.Empty();
		TokenType.Empty();
		MacKey.Empty();
		CurrentScopes.Reset();
		MacAlgorithm.Empty();
	}
	ClearLoginState();
	FTapOpenlog::SetOpenId(FString());
	FTapAppDuration::OnLogout();
	PendingCallback = nullptr;
	PendingAccessTokenCallback = FTapAccessTokenCallback();
	bLoginInProgress = false;
	PendingState.Empty();
	CodeVerifier.Empty();
	PendingScopes.Empty();
	ClearNativeAuthorization();
}

TOptional<FTapAccount> FTapLoginPlatformDesktop::GetCurrentAccount() const
{
	FScopeLock Lock(&AccountLock);
	return CurrentAccount;
}

FString FTapLoginPlatformDesktop::GetPlatformName() const
{
#if PLATFORM_WINDOWS
	return TEXT("Windows");
#elif PLATFORM_MAC
	return TEXT("Mac");
#elif PLATFORM_LINUX
	return TEXT("Linux");
#else
	return TEXT("Desktop");
#endif
}

// =============================================================================
// Scope 转换
// =============================================================================

FString FTapLoginPlatformDesktop::ScopeToString(ETapLoginScope Scope) const
{
	switch (Scope)
	{
	case ETapLoginScope::BasicInfo:       return TEXT("basic_info");
	case ETapLoginScope::PublicProfile:   return TEXT("public_profile");
	case ETapLoginScope::Email:           return TEXT("email");
	case ETapLoginScope::UserFriends:     return TEXT("user_friends");
	case ETapLoginScope::Compliance:      return TEXT("compliance");
	case ETapLoginScope::ComplianceBasic: return TEXT("compliance_basic");
	default:                              return TEXT("");
	}
}

FString FTapLoginPlatformDesktop::ScopesToParam(const TArray<ETapLoginScope>& Scopes) const
{
	TArray<FString> ScopeStrings;
	for (const ETapLoginScope& Scope : Scopes)
	{
		FString ScopeStr = ScopeToString(Scope);
		if (!ScopeStr.IsEmpty())
		{
			ScopeStrings.Add(ScopeStr);
		}
	}
	// Unity 用逗号分隔
	return FString::Join(ScopeStrings, TEXT(","));
}

TArray<FString> FTapLoginPlatformDesktop::ScopesToStringArray(const TArray<ETapLoginScope>& Scopes) const
{
	TArray<FString> Result;
	for (const ETapLoginScope& Scope : Scopes)
	{
		FString ScopeStr = ScopeToString(Scope);
		if (!ScopeStr.IsEmpty())
		{
			Result.Add(ScopeStr);
		}
	}
	return Result;
}

FTapAccessToken FTapLoginPlatformDesktop::MakeAccessToken(
	const FString& InKid,
	const FString& InTokenType,
	const FString& InMacKey,
	const FString& InMacAlgorithm,
	const TArray<FString>& InScopes) const
{
	FTapAccessToken Token;
	Token.Kid = InKid;
	Token.TokenType = InTokenType;
	Token.MacKey = InMacKey;
	Token.MacAlgorithm = InMacAlgorithm;
	Token.Scopes = InScopes;
	return Token;
}

void FTapLoginPlatformDesktop::CompleteAccessTokenAuthorizationSuccess(const FTapAccessToken& Token)
{
	FTapAccessTokenCallback Callback = MoveTemp(PendingAccessTokenCallback);
	PendingAccessTokenCallback = FTapAccessTokenCallback();
	bLoginInProgress = false;
	PendingScopes.Empty();

	if (Callback)
	{
		FTapAccessTokenResult Result;
		Result.bSuccess = true;
		Result.Token = Token;
		Callback(Result);
	}
}

void FTapLoginPlatformDesktop::CompleteAccessTokenAuthorizationFailure(int32 ErrorCode, const FString& ErrorMessage)
{
	FTapAccessTokenCallback Callback = MoveTemp(PendingAccessTokenCallback);
	PendingAccessTokenCallback = FTapAccessTokenCallback();
	bLoginInProgress = false;
	PendingScopes.Empty();

	if (Callback)
	{
		FTapAccessTokenResult Result;
		Result.ErrorCode = ErrorCode;
		Result.ErrorMessage = ErrorMessage;
		Callback(Result);
	}
}

void FTapLoginPlatformDesktop::CompleteAccessTokenAuthorizationCancelled()
{
	FTapAccessTokenCallback Callback = MoveTemp(PendingAccessTokenCallback);
	PendingAccessTokenCallback = FTapAccessTokenCallback();
	bLoginInProgress = false;
	PendingScopes.Empty();

	if (Callback)
	{
		FTapAccessTokenResult Result;
		Result.bCancelled = true;
		Callback(Result);
	}
}

TMap<FString, FString> FTapLoginPlatformDesktop::ParseQueryParams(const FString& QueryString)
{
	TMap<FString, FString> Result;
	TArray<FString> Pairs;
	QueryString.ParseIntoArray(Pairs, TEXT("&"), true);

	for (const FString& Pair : Pairs)
	{
		FString Key, Value;
		if (Pair.Split(TEXT("="), &Key, &Value))
		{
			Result.Add(Key, FGenericPlatformHttp::UrlDecode(Value));
		}
	}
	return Result;
}

// =============================================================================
// 面板回调
// =============================================================================

void FTapLoginPlatformDesktop::HandlePanelClose()
{
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Panel closed by user"));
	FTapLoginTracker::Get().TrackCancel(TEXT("loginWithScopes"), LoginSessionId);
	FTapLoginPanelManager::Get().ClosePanel();
	ClearNativeAuthorization();

	if (PendingAccessTokenCallback)
	{
		CompleteAccessTokenAuthorizationCancelled();
		return;
	}

	if (PendingCallback.GetInterface())
	{
		PendingCallback->OnLoginCancelled();
		PendingCallback = nullptr;
	}
	bLoginInProgress = false;
}

void FTapLoginPlatformDesktop::HandlePanelAuthSuccess(const FTapTokenData& TokenData, bool bWebLogin)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Panel QR auth success - kid: %s"), TEXT("[REDACTED]"));
	CurrentLoginType = bWebLogin ? FTapLoginTracker::LOGIN_TYPE_BROWSER : FTapLoginTracker::LOGIN_TYPE_CODE;
	FTapLoginPanelManager::Get().ClosePanel();

	const TArray<FString> AuthorizedScopes = TokenData.Scopes;

	if (PendingAccessTokenCallback)
	{
		FTapLoginTracker::Get().TrackSuccess(TEXT("loginWithScopes"), LoginSessionId, CurrentLoginType);
		UE_LOG(LogTapLogin, Log,
			TEXT("[Desktop] Token-only QR auth success - no profile fetch, no login state save"));
		CompleteAccessTokenAuthorizationSuccess(MakeAccessToken(
			TokenData.Kid,
			TokenData.TokenType,
			TokenData.MacKey,
			TokenData.MacAlgorithm,
			AuthorizedScopes));
		return;
	}

	{
		FScopeLock Lock(&AccountLock);
		CurrentScopes = AuthorizedScopes;
		TokenType = TokenData.TokenType;
	}
	FetchUserInfoWithToken(TokenData.Kid, TokenData.AccessToken, TokenData.MacKey, TokenData.MacAlgorithm);
}

// =============================================================================
// TapTap 客户端授权码换取 Token (参照 Unity LoginService.cs)
// =============================================================================

void FTapLoginPlatformDesktop::ExchangeCodeForToken(const FString& Code)
{
	// 参照 Unity LoginService.Authorize: POST form-urlencoded
	FString TokenEndpoint = GetWebHost() + TEXT("/oauth2/v1/token");

	FString FormBody = FString::Printf(
		TEXT("client_id=%s&grant_type=authorization_code&secret_type=hmac-sha-1&code=%s&redirect_uri=%s&code_verifier=%s"),
		*FGenericPlatformHttp::UrlEncode(ClientID),
		*FGenericPlatformHttp::UrlEncode(Code),
		*FGenericPlatformHttp::UrlEncode(RedirectURI),
		*FGenericPlatformHttp::UrlEncode(CodeVerifier)
	);

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

	FTapHttpRequest::Request(
		TEXT("POST"),
		TokenEndpoint,
		Headers,
		FormBody,
		FTapHttpRequestCompleteDelegate::CreateLambda([this](const FTapHttpResponse& Result)
		{
			// HTTP 回调可能在非游戏线程, 捕获数据后切回游戏线程
			bool bSuccess = Result.IsSuccess();
			FString ErrorMessage = Result.ErrorMessage;
			int32 StatusCode = Result.StatusCode;
			TSharedPtr<FJsonObject> JsonObject = Result.JsonObject;

			AsyncTask(ENamedThreads::GameThread, [this, bSuccess, ErrorMessage, StatusCode, JsonObject]()
			{
				if (bSuccess && JsonObject.IsValid())
				{
					ClearNativeAuthorization();
					// 服务端响应可能有 data 包装层: {"data":{...}, "success":true}
					const TSharedPtr<FJsonObject>* DataObj = nullptr;
					TSharedPtr<FJsonObject> Source = JsonObject;
					if (JsonObject->TryGetObjectField(TEXT("data"), DataObj) && DataObj && (*DataObj).IsValid())
					{
						Source = *DataObj;
					}

					// 解析 MAC Token (参照 Unity TokenData)
					const FString NewTokenKid = Source->GetStringField(TEXT("kid"));
					const FString NewAccessToken = Source->GetStringField(TEXT("access_token"));
					FString NewTokenType;
					Source->TryGetStringField(TEXT("token_type"), NewTokenType);
					const FString NewMacKey = Source->GetStringField(TEXT("mac_key"));
					const FString NewMacAlgorithm = Source->GetStringField(TEXT("mac_algorithm"));

					// Parse OAuth scope only from the token response.
					TArray<FString> AuthorizedScopes;
					FString ScopeStr;
					if (Source->TryGetStringField(TEXT("scope"), ScopeStr) && !ScopeStr.IsEmpty())
					{
						ScopeStr.ParseIntoArray(AuthorizedScopes, TEXT(" "), /*CullEmpty*/ true);
					}
					UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Token response scopes: %s"),
						*FString::Join(AuthorizedScopes, TEXT(",")));

					if (PendingAccessTokenCallback)
					{
						FTapLoginPanelManager::Get().ClosePanel();
						FTapLoginTracker::Get().TrackSuccess(TEXT("loginWithScopes"), LoginSessionId, CurrentLoginType);
						UE_LOG(LogTapLogin, Log,
							TEXT("[Desktop] Token-only client auth success - no profile fetch, no login state save"));
						CompleteAccessTokenAuthorizationSuccess(MakeAccessToken(
							NewTokenKid,
							NewTokenType,
							NewMacKey,
							NewMacAlgorithm,
							AuthorizedScopes));
						return;
					}
					{
						FScopeLock Lock(&AccountLock);
						TokenKid = NewTokenKid;
						AccessToken = NewAccessToken;
						TokenType = NewTokenType;
						MacKey = NewMacKey;
						MacAlgorithm = NewMacAlgorithm;
						CurrentScopes = AuthorizedScopes;
					}

					UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Token exchange OK - kid: %s, algo: %s, scopes: %s"),
						TEXT("[REDACTED]"), *NewMacAlgorithm, *FString::Join(AuthorizedScopes, TEXT(",")));
					FetchUserInfo();
				}
				else
				{
					UE_LOG(LogTapLogin, Error, TEXT("[Desktop] Token exchange failed: %s (HTTP %d)"),
						*ErrorMessage, StatusCode);
					FTapLoginTracker::Get().TrackFailure(TEXT("loginWithScopes"), LoginSessionId,
						CurrentLoginType, 80000, TEXT("UnKnow Error"));
					if (PendingAccessTokenCallback)
					{
						CompleteAccessTokenAuthorizationFailure(StatusCode, ErrorMessage);
						return;
					}
					if (PendingCallback.GetInterface())
					{
						PendingCallback->OnLoginFailed(-1, ErrorMessage);
						PendingCallback = nullptr;
					}
					bLoginInProgress = false;
				}
			});
		}),
		30.0f
	);
}

void FTapLoginPlatformDesktop::FetchUserInfo()
{
	// 网页登录成功 → 关闭面板 → 拉取用户信息
	FTapLoginPanelManager::Get().ClosePanel();

	FString CurrentKid;
	FString CurrentAccessToken;
	FString CurrentMacKey;
	FString CurrentMacAlgorithm;
	{
		FScopeLock Lock(&AccountLock);
		CurrentKid = TokenKid;
		CurrentAccessToken = AccessToken;
		CurrentMacKey = MacKey;
		CurrentMacAlgorithm = MacAlgorithm;
	}
	FetchUserInfoWithToken(CurrentKid, CurrentAccessToken, CurrentMacKey, CurrentMacAlgorithm);
}

void FTapLoginPlatformDesktop::FetchUserInfoWithToken(
	const FString& InKid, const FString& InAccessToken,
	const FString& InMacKey, const FString& InMacAlgorithm,
	uint64 RequestGeneration)
{
	uint64 EffectiveGeneration = RequestGeneration;
	{
		FScopeLock Lock(&AccountLock);
		if (EffectiveGeneration == 0)
		{
			EffectiveGeneration = StateGeneration;
		}
		TokenKid = InKid;
		AccessToken = InAccessToken;
		MacKey = InMacKey;
		MacAlgorithm = InMacAlgorithm;
	}

	// 参照 Unity LoginService.GetProfile: 根据实际授权 scope 选择资料接口。
	// basic_info-only 登录不能请求 profile/v1，否则会再次隐式依赖 public_profile。
	const bool bHasPublicProfile = [&]()
	{
		FScopeLock Lock(&AccountLock);
		return CurrentScopes.Contains(TEXT("public_profile"));
	}();
	const FString ProfilePath = bHasPublicProfile
		? TEXT("/account/profile/v1?client_id=")
		: TEXT("/account/basic-info/v1?client_id=");
	FString ProfileURL = GetApiHost() + ProfilePath + ClientID;

	// 解析 URL 获取 host 和 path (用于 MAC 签名)
	FString Host;
	FString PathAndQuery;
	{
		FString UrlWithoutScheme = ProfileURL;
		UrlWithoutScheme.RemoveFromStart(TEXT("https://"));
		int32 SlashIndex;
		if (UrlWithoutScheme.FindChar(TEXT('/'), SlashIndex))
		{
			Host = UrlWithoutScheme.Left(SlashIndex);
			PathAndQuery = UrlWithoutScheme.Mid(SlashIndex);
		}
	}

	// 生成 MAC 签名 (参照 Unity LoginService.GetAuthorizationHeader)
	// 时间必须用校准过的 FTapHttpTime, 与 X-Tap-Ts 同源, 否则本地时钟漂移会让服务端 MAC 校验失败.
	int64 Timestamp = FTapHttpTime::GetCurrentTime();
	FString MacAuth = FTapLoginCryptoUtils::GenerateMacSignature(
		InKid, InMacKey, InMacAlgorithm,
		TEXT("GET"), PathAndQuery, Host, TEXT("443"), Timestamp
	);

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), TEXT("MAC ") + MacAuth);

	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Fetching user profile..."));

		FTapHttpRequest::Request(
			TEXT("GET"),
			ProfileURL,
			Headers,
			TEXT(""),
			FTapHttpRequestCompleteDelegate::CreateLambda([this, EffectiveGeneration](const FTapHttpResponse& Result)
		{
			// HTTP 回调可能在非游戏线程, 捕获数据后切回游戏线程
			bool bSuccess = Result.IsSuccess();
			FString ErrorMessage = Result.ErrorMessage;
			int32 StatusCode = Result.StatusCode;
			TSharedPtr<FJsonObject> JsonObject = Result.JsonObject;

			AsyncTask(ENamedThreads::GameThread, [this, bSuccess, ErrorMessage, StatusCode, JsonObject, EffectiveGeneration]()
			{
				{
					FScopeLock Lock(&AccountLock);
					if (EffectiveGeneration != StateGeneration || !bCacheUserSameWithTapClient)
					{
						UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Ignoring stale profile response"));
						return;
					}
				}
				if (bSuccess && JsonObject.IsValid())
				{
					// 解析 ProfileData (参照 Unity ProfileData)
					TSharedPtr<FJsonObject> Data = JsonObject->GetObjectField(TEXT("data"));
					if (!Data.IsValid())
					{
						Data = JsonObject;
					}

					FTapAccount Account;
					{
						FScopeLock Lock(&AccountLock);
						Account.AccessToken.Kid = TokenKid;
						Account.AccessToken.MacKey = MacKey;
						Account.AccessToken.MacAlgorithm = MacAlgorithm;
						Account.AccessToken.TokenType = TokenType;
						Account.AccessToken.Scopes = CurrentScopes;
					}
					Data->TryGetStringField(TEXT("openid"), Account.OpenID);
					Data->TryGetStringField(TEXT("unionid"), Account.UnionID);
					Data->TryGetStringField(TEXT("name"), Account.Name);
					Data->TryGetStringField(TEXT("avatar"), Account.Avatar);
					Data->TryGetStringField(TEXT("email"), Account.Email);

					UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Login success - Name: %s, OpenID: %s"),
						TEXT("[REDACTED]"), *Account.OpenID);

					FTapLoginTracker::Get().TrackSuccess(TEXT("loginWithScopes"), LoginSessionId, CurrentLoginType);
					bLoginInProgress = false;

							{
								FScopeLock Lock(&AccountLock);
								CurrentAccount = Account;
							}
							FTapOpenlog::SetOpenId(Account.OpenID);
							FTapAppDuration::OnLogin(Account.OpenID);
							SaveLoginState();
							FTapLoginInternal::BroadcastLoginStateChanged(Account);

					if (PendingCallback.GetInterface())
					{
						PendingCallback->OnLoginSuccess(Account);
						PendingCallback = nullptr;
					}
				}
				else
				{
					UE_LOG(LogTapLogin, Error, TEXT("[Desktop] Profile fetch failed (HTTP %d): %s"),
						StatusCode, *ErrorMessage);
					FTapLoginTracker::Get().TrackFailure(TEXT("loginWithScopes"), LoginSessionId,
						CurrentLoginType, 80000, TEXT("UnKnow Error"));
					if (PendingCallback.GetInterface())
					{
						PendingCallback->OnLoginFailed(-1, ErrorMessage);
						PendingCallback = nullptr;
					}
					bLoginInProgress = false;
				}
			});
		}),
		30.0f
	);
}

void FTapLoginPlatformDesktop::ClearNativeAuthorization()
{
	PendingState.Empty();
	CodeVerifier.Empty();
	CodeChallenge.Empty();
	RedirectURI.Empty();
	PendingScopes.Empty();
}
void FTapLoginPlatformDesktop::SaveLoginState()
{
	TOptional<FTapAccount> AccountSnapshot;
	{
		FScopeLock Lock(&AccountLock);
		AccountSnapshot = CurrentAccount;
	}

	if (AccountSnapshot.IsSet())
	{
		const FTapAccount& Acct = AccountSnapshot.GetValue();
		LoginPrefs.Set(TEXT("client_id"), ClientID);
		LoginPrefs.Set(TEXT("kid"), Acct.AccessToken.Kid);
		// Unity PC refreshes with kid; do not persist the raw OAuth access_token.
		LoginPrefs.Remove(TEXT("access_token"));
		LoginPrefs.Set(TEXT("token_type"), Acct.AccessToken.TokenType);
		LoginPrefs.Set(TEXT("mac_key"), Acct.AccessToken.MacKey);
		LoginPrefs.Set(TEXT("mac_algorithm"), Acct.AccessToken.MacAlgorithm);
		LoginPrefs.Set(TEXT("scope"), FString::Join(Acct.AccessToken.Scopes, TEXT(" ")));
		LoginPrefs.Set(TEXT("openid"), Acct.OpenID);
		LoginPrefs.Set(TEXT("unionid"), Acct.UnionID);
		LoginPrefs.Set(TEXT("name"), Acct.Name);
		LoginPrefs.Set(TEXT("avatar"), Acct.Avatar);
	}

	LoginPrefs.Save();
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Login state saved (scopes: %s)"),
		AccountSnapshot.IsSet()
			? *FString::Join(AccountSnapshot->AccessToken.Scopes, TEXT(","))
			: TEXT(""));
}

void FTapLoginPlatformDesktop::LoadLoginState()
{
	if (LoginPrefs.GetString(TEXT("client_id")) != ClientID) return;
	FString SavedKid = LoginPrefs.GetString(TEXT("kid"));
	if (SavedKid.IsEmpty())
	{
		UE_LOG(LogTapLogin, Log, TEXT("[Desktop] No saved login state"));
		return;
	}

	{
		FScopeLock Lock(&AccountLock);
		TokenKid = SavedKid;
		// Raw access_token is intentionally not restored or persisted; refresh uses TokenKid.
		AccessToken.Empty();
		TokenType = LoginPrefs.GetString(TEXT("token_type"));
		MacKey = LoginPrefs.GetString(TEXT("mac_key"));
		MacAlgorithm = LoginPrefs.GetString(TEXT("mac_algorithm"));

		CurrentScopes.Reset();
		const FString ScopeStr = LoginPrefs.GetString(TEXT("scope"));
		if (!ScopeStr.IsEmpty())
		{
			ScopeStr.ParseIntoArray(CurrentScopes, TEXT(" "), true);
		}
	}

	FTapAccount Account;
	{
		FScopeLock Lock(&AccountLock);
		Account.AccessToken.Kid = TokenKid;
		Account.AccessToken.MacKey = MacKey;
		Account.AccessToken.MacAlgorithm = MacAlgorithm;
		Account.AccessToken.TokenType = TokenType;
		Account.AccessToken.Scopes = CurrentScopes;
	}
	Account.OpenID = LoginPrefs.GetString(TEXT("openid"));
	Account.UnionID = LoginPrefs.GetString(TEXT("unionid"));
	Account.Name = LoginPrefs.GetString(TEXT("name"));
	Account.Avatar = LoginPrefs.GetString(TEXT("avatar"));
	{
		FScopeLock Lock(&AccountLock);
		CurrentAccount = Account;
	}

	FTapOpenlog::SetOpenId(Account.OpenID);
	FTapAppDuration::OnLogin(Account.OpenID);
	FTapLoginInternal::BroadcastLoginStateChanged(Account);

	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Login state restored - Name: %s, OpenID: %s, Scopes: %s"),
		TEXT("[REDACTED]"), *Account.OpenID, *FString::Join(Account.AccessToken.Scopes, TEXT(",")));
}

void FTapLoginPlatformDesktop::ClearLoginState()
{
	LoginPrefs.Clear();
	LoginPrefs.Save();
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Login state cleared"));
}

// =============================================================================
// FastAuth 支撑 (用于 TapCompliance 等需要复用登录态的模块)
// =============================================================================

bool FTapLoginPlatformDesktop::HasScope(const FString& Scope) const
{
	if (Scope.IsEmpty()) return false;
	FScopeLock Lock(&AccountLock);
	for (const FString& S : CurrentScopes)
	{
		if (S.Equals(Scope))
		{
			return true;
		}
	}
	return false;
}

TArray<FString> FTapLoginPlatformDesktop::GetCurrentScopes() const
{
	FScopeLock Lock(&AccountLock);
	return CurrentScopes;
}

FString FTapLoginPlatformDesktop::BuildAuthorizationHeader(
	const FString& Method,
	const FString& Uri,
	const FString& Host,
	const FString& Port,
	int64          Timestamp) const
{
	FString CurrentKid;
	FString CurrentMacKey;
	FString CurrentMacAlgorithm;
	{
		FScopeLock Lock(&AccountLock);
		CurrentKid = TokenKid;
		CurrentMacKey = MacKey;
		CurrentMacAlgorithm = MacAlgorithm;
	}

	if (CurrentKid.IsEmpty() || CurrentMacKey.IsEmpty() || CurrentMacAlgorithm.IsEmpty())
	{
		UE_LOG(LogTapLogin, Warning,
			TEXT("[Desktop] BuildAuthorizationHeader: not logged in or missing MAC fields"));
		return FString();
	}

	const int64 Ts = Timestamp > 0
		? Timestamp
		: FTapHttpTime::GetCurrentTime();

	const FString Sig = FTapLoginCryptoUtils::GenerateMacSignature(
		CurrentKid, CurrentMacKey, CurrentMacAlgorithm,
		Method, Uri, Host, Port, Ts);

	if (Sig.IsEmpty())
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Desktop] BuildAuthorizationHeader: GenerateMacSignature returned empty"));
		return FString();
	}

	// GenerateMacSignature 返回的是 id="...",ts="...",nonce="...",mac="..."
	// 加 "MAC " 前缀构成完整 Authorization header value
	return FString(TEXT("MAC ")) + Sig;
}

void FTapLoginPlatformDesktop::AuthorizeForAccessToken(
	const TArray<ETapLoginScope>& Scopes,
	FTapAccessTokenCallback Callback)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] AuthorizeForAccessToken - Scopes count: %d"), Scopes.Num());

	if (!bInitialized)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Desktop] TapLogin not initialized"));
		if (Callback)
		{
			FTapAccessTokenResult Result;
			Result.ErrorCode = -1;
			Result.ErrorMessage = TEXT("TapLogin not initialized");
			Callback(Result);
		}
		return;
	}

	if (!Callback)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Desktop] AuthorizeForAccessToken callback is null"));
		return;
	}

	if (bLoginInProgress || PendingAccessTokenCallback)
	{
		FTapAccessTokenResult Result;
		Result.ErrorCode = -1;
		Result.ErrorMessage = TEXT("Currently logging in");
		Callback(Result);
		return;
	}

	BeginLogin();
	PendingCallback = nullptr;
	PendingAccessTokenCallback = MoveTemp(Callback);
	PendingScopes = Scopes;

	LoginSessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	CurrentLoginType.Empty();
	FTapLoginTracker::Get().TrackStart(TEXT("loginWithScopes"), LoginSessionId);

	TArray<FString> ScopeStringArray = ScopesToStringArray(PendingScopes);
#if PLATFORM_WINDOWS
	if (UTapLauncher::IsPassedLauncherCheck() && StartNativeAuthorization())
	{
		UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Token-only Tap PC native authorization started"));
		return;
	}
#endif
	FTapLoginPanelManager::Get().ShowPanel(
		ClientID,
		ScopeStringArray,
		FOnLoginPanelClose::CreateRaw(this, &FTapLoginPlatformDesktop::HandlePanelClose),
		FOnLoginPanelAuthSuccess::CreateRaw(this, &FTapLoginPlatformDesktop::HandlePanelAuthSuccess)
	);

	UE_LOG(LogTapLogin, Log, TEXT("[Desktop] Token-only authorization panel shown"));
}

void FTapLoginPlatformDesktop::Authorize(
	const TArray<ETapLoginScope>& Scopes,
	TScriptInterface<ITapLoginCallback> Callback)
{
	// 已登录 + 已包含全部 scope → 短路返回当前 token
	TOptional<FTapAccount> AccountSnapshot;
	{
		FScopeLock Lock(&AccountLock);
		AccountSnapshot = CurrentAccount;
	}
	if (AccountSnapshot.IsSet())
	{
		bool bAllPresent = true;
		for (const ETapLoginScope& S : Scopes)
		{
			if (!HasScope(ScopeToString(S)))
			{
				bAllPresent = false;
				break;
			}
		}
		if (bAllPresent && Scopes.Num() > 0)
		{
			UE_LOG(LogTapLogin, Log,
				TEXT("[Desktop] Authorize: all scopes present, short-circuit"));
			if (Callback.GetInterface())
			{
				const FTapAccount& A = AccountSnapshot.GetValue();
				Callback->OnLoginSuccess(A);
			}
			return;
		}
	}

	UE_LOG(LogTapLogin, Log,
		TEXT("[Desktop] Authorize: scope upgrade required → re-trigger LoginWithScopes"));
	LoginWithScopes(Scopes, Callback);
}

#if WITH_DEV_AUTOMATION_TESTS
void FTapLoginPlatformDesktop::InjectTokenForTest(
	const FString& InKid,
	const FString& InAccessToken,
	const FString& InMacKey,
	const FString& InMacAlgorithm,
	const TArray<FString>& InScopes)
{
	{
		FScopeLock Lock(&AccountLock);
		TokenKid = InKid;
		AccessToken = InAccessToken;
		TokenType = TEXT("mac");
		MacKey = InMacKey;
		MacAlgorithm = InMacAlgorithm;
		CurrentScopes = InScopes;
	}

	// 注意: 不要在这里把 bInitialized 置为 true。
	// 否则真实游戏 Init 会走 FTapTapLogin::Initialize() 的 "already initialized" 早退分支,
	// ClientID 永远拿不到 FTapTapSdkOptions 里的值, LoginWithScopes 发出去的请求 client_id 是空串。
	// BuildAuthorizationHeader 自身只需 token 字段, 不依赖完整初始化。

	// 同时构造一个最小可用的 CurrentAccount，便于 HasScope / GetCurrentAccount 检查通过
	FTapAccount Account;
	Account.AccessToken.Kid = InKid;
	Account.AccessToken.MacKey = InMacKey;
	Account.AccessToken.MacAlgorithm = InMacAlgorithm;
	Account.AccessToken.TokenType = TEXT("mac");
	Account.AccessToken.Scopes = InScopes;
	{
		FScopeLock Lock(&AccountLock);
		CurrentAccount = Account;
	}

	UE_LOG(LogTapLogin, Warning,
		TEXT("[Desktop][TEST] Token injected: kid.len=%d mac_key.len=%d algo=%s scopes=%s"),
		InKid.Len(), InMacKey.Len(), *InMacAlgorithm,
		*FString::Join(InScopes, TEXT(",")));
}
#endif

void FTapLoginPlatformDesktop::HandleLauncherCheckCompleted(const FString& OpenId)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, OpenId]
		{
			HandleLauncherCheckCompleted(OpenId);
		});
		return;
	}
	ReconcileLauncherAccount(OpenId);
}

void FTapLoginPlatformDesktop::ReconcileLauncherAccount(const FString& OpenId)
{
	if (OpenId.IsEmpty())
	{
		return;
	}

	bool bMismatch = false;
	{
		FScopeLock Lock(&AccountLock);
		if (CurrentAccount.IsSet() && !CurrentAccount->OpenID.IsEmpty())
		{
			bMismatch = CurrentAccount->OpenID != OpenId;
		}
		bCacheUserSameWithTapClient = !bMismatch;
		if (bMismatch)
		{
			++StateGeneration;
		}
	}

	if (bMismatch)
	{
		UE_LOG(LogTapLogin, Warning,
			TEXT("[Desktop] Cached account does not match TapTap PC OpenID; clearing login state"));
		Logout();
	}
}

void FTapLoginPlatformDesktop::RefreshCachedToken()
{
	FString RefreshToken;
	uint64 RequestGeneration = 0;
	{
		FScopeLock Lock(&AccountLock);
		if (!CurrentAccount.IsSet() || TokenKid.IsEmpty() || !bCacheUserSameWithTapClient)
		{
			return;
		}
		// Match Unity PC: refresh with the public AccessToken.kid, not a persisted raw token.
		RefreshToken = TokenKid;
		RequestGeneration = StateGeneration;
	}

	const FString TokenEndpoint = GetWebHost() + TEXT("/oauth2/v1/token");
	const FString Info = TEXT("{\"device_id\":\"desktop\"}");
	const FString FormBody = FString::Printf(
		TEXT("client_id=%s&grant_type=refresh_token&token=%s&token_type_hint=access_token&platform=unity&info=%s"),
		*FGenericPlatformHttp::UrlEncode(ClientID),
		*FGenericPlatformHttp::UrlEncode(RefreshToken),
		*FGenericPlatformHttp::UrlEncode(Info));
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	FTapHttpRequest::Request(
		TEXT("POST"), TokenEndpoint, Headers, FormBody,
		FTapHttpRequestCompleteDelegate::CreateLambda([this, RequestGeneration](const FTapHttpResponse& Result)
		{
			const bool bSuccess = Result.IsSuccess();
			const int32 StatusCode = Result.StatusCode;
			const FString ErrorMessage = Result.ErrorMessage;
			const TSharedPtr<FJsonObject> JsonObject = Result.JsonObject;
			AsyncTask(ENamedThreads::GameThread, [this, RequestGeneration, bSuccess, StatusCode, ErrorMessage, JsonObject]()
			{
				FTapHttpResponse Snapshot;
				Snapshot.bSuccess = bSuccess;
				Snapshot.StatusCode = StatusCode;
				Snapshot.ErrorMessage = ErrorMessage;
				Snapshot.JsonObject = JsonObject;
				HandleRefreshTokenResponse(RequestGeneration, Snapshot);
			});
		}), 30.0f);
}

void FTapLoginPlatformDesktop::HandleRefreshTokenResponse(
	uint64 RequestGeneration, const FTapHttpResponse& Result)
{
	bool bCurrent = false;
	{
		FScopeLock Lock(&AccountLock);
		bCurrent = RequestGeneration == StateGeneration && bCacheUserSameWithTapClient;
	}
	if (!bCurrent)
	{
		return;
	}

	if (!Result.IsSuccess() || !Result.JsonObject.IsValid())
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Desktop] Cached token refresh failed (HTTP %d): %s"),
			Result.StatusCode, *Result.ErrorMessage);
		const FString Error = Result.JsonObject.IsValid()
			? Result.JsonObject->GetStringField(TEXT("error"))
			: FString();
		if (Error == FTapHttpErrorConstants::ERROR_INVALID_GRANT
			|| Error == FTapHttpErrorConstants::ERROR_TOKEN_EXPIRED
			|| Error == FTapHttpErrorConstants::ERROR_TOKEN_INVALID
			|| Result.StatusCode == 401)
		{
			Logout();
		}
		return;
	}

	TSharedPtr<FJsonObject> Source;
	const TSharedPtr<FJsonObject>* Data = nullptr;
	if (Result.JsonObject->TryGetObjectField(TEXT("data"), Data) && Data)
	{
		Source = *Data;
	}
	if (!Source.IsValid())
	{
		Source = Result.JsonObject;
	}

	FString NewKid;
	FString NewAccessToken;
	FString NewTokenType;
	FString NewMacKey;
	FString NewMacAlgorithm;
	if (!Source->TryGetStringField(TEXT("kid"), NewKid)
		|| !Source->TryGetStringField(TEXT("access_token"), NewAccessToken)
		|| !Source->TryGetStringField(TEXT("mac_key"), NewMacKey)
		|| !Source->TryGetStringField(TEXT("mac_algorithm"), NewMacAlgorithm))
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Desktop] Cached token refresh response is missing MAC fields"));
		return;
	}
	Source->TryGetStringField(TEXT("token_type"), NewTokenType);
	TArray<FString> AuthorizedScopes;
	FString ScopeString;
	if (Source->TryGetStringField(TEXT("scope"), ScopeString))
	{
		ScopeString.ParseIntoArray(AuthorizedScopes, TEXT(" "), true);
	}

	{
		FScopeLock Lock(&AccountLock);
		if (RequestGeneration != StateGeneration || !bCacheUserSameWithTapClient)
		{
			return;
		}
		TokenKid = NewKid;
		AccessToken = NewAccessToken;
		TokenType = NewTokenType;
		MacKey = NewMacKey;
		MacAlgorithm = NewMacAlgorithm;
		CurrentScopes = AuthorizedScopes;
	}
	FetchUserInfoWithToken(NewKid, NewAccessToken, NewMacKey, NewMacAlgorithm, RequestGeneration);
}

#if PLATFORM_WINDOWS
bool FTapLoginPlatformDesktop::StartNativeAuthorization()
{
	if (!FTapLoginClientBridge::IsAvailable())
	{
		return false;
	}

	CodeVerifier = FTapLoginCryptoUtils::GenerateCodeVerifier();
	CodeChallenge = FTapLoginCryptoUtils::GenerateCodeChallenge(CodeVerifier);
	PendingState = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	RedirectURI = TEXT("tapoauth://authorize");
	CurrentLoginType = TEXT("pc_client");

	const FString SdkUa = FString::Printf(TEXT("client_id=%s&uuid=%s"), *ClientID, *FTapDeviceId::Get());
	const FString Info = TEXT("{\"device_id\":\"desktop\"}");
	return FTapLoginClientBridge::Start(
		ScopesToParam(PendingScopes), TEXT("code"), RedirectURI,
		CodeChallenge, PendingState, TEXT("S256"), TapLogin_VERSION,
		SdkUa, Info,
		FTapLoginClientAuthorizeComplete::CreateRaw(
			this, &FTapLoginPlatformDesktop::HandleNativeAuthorizationComplete));
}

void FTapLoginPlatformDesktop::HandleNativeAuthorizationFailure(const FString& Message)
{
	UE_LOG(LogTapLogin, Error, TEXT("[Desktop] Tap PC native authorization failed: %s"), *Message);
	FTapLoginTracker::Get().TrackFailure(TEXT("loginWithScopes"), LoginSessionId, CurrentLoginType, -1, Message);
	if (PendingAccessTokenCallback)
	{
		CompleteAccessTokenAuthorizationFailure(-1, Message);
	}
	else if (PendingCallback.GetInterface())
	{
		PendingCallback->OnLoginFailed(-1, Message);
		PendingCallback = nullptr;
	}
	bLoginInProgress = false;
}

void FTapLoginPlatformDesktop::HandleNativeAuthorizationComplete(bool bCancelled, const FString& CallbackUri)
{
	if (bCancelled)
	{
		FTapLoginTracker::Get().TrackCancel(TEXT("loginWithScopes"), LoginSessionId);
		if (PendingAccessTokenCallback)
		{
			CompleteAccessTokenAuthorizationCancelled();
		}
		else if (PendingCallback.GetInterface())
		{
			PendingCallback->OnLoginCancelled();
			PendingCallback = nullptr;
		}
		bLoginInProgress = false;
		return;
	}

	int32 QueryIndex = INDEX_NONE;
	const FString Query = CallbackUri.FindChar(TEXT('?'), QueryIndex)
		? CallbackUri.Mid(QueryIndex + 1)
		: FString();
	const TMap<FString, FString> Params = ParseQueryParams(Query);
	const FString Error = Params.FindRef(TEXT("error"));
	if (Params.FindRef(TEXT("state")) != PendingState)
	{
		HandleNativeAuthorizationFailure(TEXT("authorization state mismatch"));
		return;
	}
	if (!Error.IsEmpty())
	{
		if (Error == FTapHttpErrorConstants::ERROR_ACCESS_DENIED)
		{
			if (PendingAccessTokenCallback)
			{
				CompleteAccessTokenAuthorizationCancelled();
			}
			else if (PendingCallback.GetInterface())
			{
				PendingCallback->OnLoginCancelled();
				PendingCallback = nullptr;
			}
			bLoginInProgress = false;
			FTapLoginTracker::Get().TrackCancel(TEXT("loginWithScopes"), LoginSessionId);
			return;
		}
		HandleNativeAuthorizationFailure(Error);
		return;
	}
	const FString Code = Params.FindRef(TEXT("code"));
	if (Code.IsEmpty())
	{
		HandleNativeAuthorizationFailure(TEXT("authorization callback did not contain a code"));
		return;
	}
	ExchangeCodeForToken(Code);
}
#endif

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
