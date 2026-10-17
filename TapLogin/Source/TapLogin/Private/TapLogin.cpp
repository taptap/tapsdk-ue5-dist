// Copyright TapTap. All Rights Reserved.

#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "TapLoginModule.h"
#include "Async/Async.h"
#include "TapLoginAuthorizationServiceImpl.h"
#include "Internal/TapSdkChecker.h"
#include "Desktop/TapLoginCryptoUtils.h"
#include "Internal/Http/TapHttp.h"
#include "Internal/Http/TapHttpTime.h"
#include "Platform/ITapLoginPlatform.h"
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Desktop/TapDeepLinkHandler.h"
#endif


// 初始化静态成员
TOptional<FTapAccount> FTapTapLogin::CurrentAccount;
FCriticalSection FTapTapLogin::CurrentAccountLock;
bool FTapTapLogin::bIsInitialized = false;
TSharedPtr<ITapLoginPlatform> FTapTapLogin::PlatformImpl = nullptr;

FTapLoginStateChangedDelegate& FTapTapLogin::OnLoginStateChanged()
{
	static FTapLoginStateChangedDelegate Delegate;
	return Delegate;
}

void FTapTapLogin::BroadcastLoginStateChanged(const TOptional<FTapAccount>& Account)
{
	const TOptional<FTapAccount> Snapshot = Account;
	if (IsInGameThread())
	{
		OnLoginStateChanged().Broadcast(Snapshot);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [Snapshot]
		{
			OnLoginStateChanged().Broadcast(Snapshot);
		});
	}
}

void FTapTapLogin::Initialize()
{
	// 兜底重注册全局 AuthorizationService:
	// TapLoginModule::StartupModule 在模块加载时注册过一次, 但 UE 启动时会自动跑
	// SmokeFilter 测试 (例如 TapBasic.Http.Pure.Auth.GlobalRegistry), 这些测试 cleanup
	// 阶段会 SetAuthorizationService(nullptr) 把全局清空; 后续业务请求拿不到
	// Authorization 头会 401. 这里每次 Init 都重注册一次, 保证 service 一定可用.
	FTapHttp::SetAuthorizationService(MakeShared<FTapLoginAuthorizationServiceImpl>());

	// 以平台实例的真实状态为准, 不依赖顶层静态 bIsInitialized:
	// 该静态在 UE Editor 跨 PIE / Live Coding 部分模块重载时可能残留 stale true,
	// 若仍用它做短路, Desktop::Initialize() 会被跳过, 平台实例 bInitialized 永远为 false,
	// 进而 LoginWithScopes 报 "[Desktop] TapLogin not initialized".
	if (PlatformImpl.IsValid() && PlatformImpl->IsInitialized())
	{
		UE_LOG(LogTapLogin, Warning, TEXT("TapLogin is already initialized"));
		bIsInitialized = true; // 同步顶层 flag, 避免 stale false
		return;
	}

	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin Initialization"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));

	// 创建平台特定的实现
	PlatformImpl = FTapLoginPlatformFactory::CreatePlatform();
	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapLogin, Error, TEXT("Failed to create platform implementation"));
		return;
	}

	// 初始化平台实现
	PlatformImpl->Initialize();

	bIsInitialized = PlatformImpl->IsInitialized();

	UE_LOG(LogTapLogin, Log, TEXT("TapLogin Version: %s"), *GetVersion());
	UE_LOG(LogTapLogin, Log, TEXT("Platform: %s"), *PlatformImpl->GetPlatformName());
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin initialized: %s"), bIsInitialized ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
}

#if PLATFORM_WINDOWS
void FTapLoginInternal::HandleWindowsDeepLinkURL(const FString& URL)
{
	FTapDeepLinkHandler::Get().HandleURL(URL);
}
#endif

bool FTapTapLogin::IsInitialized()
{
	// 真正问平台层, 不只看顶层 flag (后者跨 PIE / Live Coding 可能 stale)
	return PlatformImpl.IsValid() && PlatformImpl->IsInitialized();
}

void FTapTapLogin::LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) { if(Callback.GetInterface())Callback->OnLoginFailed(-1, TEXT("TapSDK not initialized")); return; }

	UE_LOG(LogTapLogin, Log, TEXT("LoginWithScopes called with %d scopes"), Scopes.Num());

	if (!IsInitialized())
	{
		UE_LOG(LogTapLogin, Error, TEXT("TapLogin not initialized"));
		if (Callback.GetInterface())
		{
			Callback->OnLoginFailed(-1, TEXT("TapLogin not initialized"));
		}
		return;
	}

	// 委托给平台实现
	PlatformImpl->LoginWithScopes(Scopes, Callback);
}

void FTapTapLogin::Logout()
{
	if (!FTapSdkChecker::CheckInitialize()) return;

	UE_LOG(LogTapLogin, Log, TEXT("Logout called"));

	// 清除当前账号（向后兼容）
	{
		FScopeLock Lock(&CurrentAccountLock);
		CurrentAccount.Reset();
	}
	if (IsInitialized())
	{
		// 委托给平台实现
		PlatformImpl->Logout();
	}
	BroadcastLoginStateChanged(TOptional<FTapAccount>());
}

TOptional<FTapAccount> FTapTapLogin::ReadCurrentAccount()
{
	if (IsInitialized())
	{
		return PlatformImpl->GetCurrentAccount();
	}

	FScopeLock Lock(&CurrentAccountLock);
	return CurrentAccount;
}

TOptional<FTapAccount> FTapTapLogin::GetCurrentTapAccount()
{
	if (!FTapSdkChecker::CheckInitialize()) return TOptional<FTapAccount>();
	return ReadCurrentAccount();
}


TOptional<FTapAccount> FTapTapLogin::PeekCachedAccount()
{
	if (!FTapSdkChecker::CheckLocalInitialize(false)) return TOptional<FTapAccount>();
	return ReadCurrentAccount();
}

FString FTapTapLogin::GetVersion()
{
	return TEXT("1.0.0");
}

// =========================================================
// FastAuth 支撑
// =========================================================

bool FTapTapLogin::HasScope(const FString& Scope)
{
	if (!IsInitialized()) return false;
	return PlatformImpl->HasScope(Scope);
}

TArray<FString> FTapTapLogin::GetCurrentScopes()
{
	if (!IsInitialized()) return {};
	return PlatformImpl->GetCurrentScopes();
}

FString FTapTapLogin::BuildAuthorizationHeader(
	const FString& Method,
	const FString& Uri,
	const FString& Host,
	const FString& Port,
	int64          Timestamp)
{
	// 不走 IsInitialized(): 自动化测试通过 InjectTokenForTest 直接注入 token,
	// 此时平台实例存在且有完整 MAC 字段, 但 bInitialized 仍为 false (避免影响后续真实 Init)。
	// 真正的"未登录"判定由平台实现里的 TokenKid/MacKey/MacAlgorithm 为空检查兜底。
	if (!PlatformImpl.IsValid()) return FString();
	return PlatformImpl->BuildAuthorizationHeader(Method, Uri, Host, Port, Timestamp);
}

FString FTapTapLogin::BuildAuthorizationHeaderForToken(
	const FTapAccessToken& Token,
	const FString& Method,
	const FString& Uri,
	const FString& Host,
	const FString& Port,
	int64          Timestamp)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	if (!Token.IsValidForMac())
	{
		UE_LOG(LogTapLogin, Warning,
			TEXT("[TapLogin] BuildAuthorizationHeaderForToken: missing MAC token fields"));
		return FString();
	}

	const int64 Ts = Timestamp > 0
		? Timestamp
		: FTapHttpTime::GetCurrentTime();

	const FString Sig = FTapLoginCryptoUtils::GenerateMacSignature(
		Token.Kid,
		Token.MacKey,
		Token.MacAlgorithm,
		Method,
		Uri,
		Host,
		Port,
		Ts);

	if (Sig.IsEmpty())
	{
		UE_LOG(LogTapLogin, Warning,
			TEXT("[TapLogin] BuildAuthorizationHeaderForToken: GenerateMacSignature returned empty"));
		return FString();
	}

	return FString(TEXT("MAC ")) + Sig;
#else
	return FString();
#endif
}

void FTapLoginInternal::AuthorizeForAccessToken(
	const TArray<ETapLoginScope>& Scopes,
	FTapAccessTokenCallback Callback)
{
	if (!FTapSdkChecker::CheckInitialize())
	{
		if (Callback)
		{
			FTapAccessTokenResult Result;
			Result.ErrorCode = -1;
			Result.ErrorMessage = TEXT("TapSDK not initialized");
			Callback(Result);
		}
		return;
	}

	if (!IsInitialized())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[TapLogin] AuthorizeForAccessToken: not initialized"));
		if (Callback)
		{
			FTapAccessTokenResult Result;
			Result.ErrorCode = -1;
			Result.ErrorMessage = TEXT("TapLogin not initialized");
			Callback(Result);
		}
		return;
	}

	UE_LOG(LogTapLogin, Log, TEXT("[TapLogin] AuthorizeForAccessToken called with %d scopes"), Scopes.Num());
	FTapTapLogin::PlatformImpl->AuthorizeForAccessToken(Scopes, MoveTemp(Callback));
}

void FTapTapLogin::Authorize(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) { if(Callback.GetInterface())Callback->OnLoginFailed(-1, TEXT("TapSDK not initialized")); return; }

	if (!IsInitialized())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[TapLogin] Authorize: not initialized"));
		if (Callback.GetInterface())
		{
			Callback->OnLoginFailed(-1, TEXT("TapLogin not initialized"));
		}
		return;
	}

	UE_LOG(LogTapLogin, Log, TEXT("[TapLogin] Authorize called with %d scopes"), Scopes.Num());
	PlatformImpl->Authorize(Scopes, Callback);
}

#if WITH_DEV_AUTOMATION_TESTS
void FTapTapLogin::InjectTokenForTest(
	const FString& Kid,
	const FString& AccessToken,
	const FString& MacKey,
	const FString& MacAlgorithm,
	const TArray<FString>& Scopes)
{
	// 测试场景下绕开 Initialize():
	//   已经手动注入完整 MAC token (kid+mac_key+algo)，BuildAuthorizationHeader
	//   只用这些字段，不需要 Initialize 干的 LoadLoginState / DeepLink 等。
	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapLogin, Warning,
			TEXT("[TapLogin][TEST] InjectTokenForTest: creating PlatformImpl directly (bypass Initialize)"));
		PlatformImpl = FTapLoginPlatformFactory::CreatePlatform();
	}
	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapLogin, Error,
			TEXT("[TapLogin][TEST] InjectTokenForTest: failed to create platform impl, abort"));
		return;
	}

	// 不置 bIsInitialized / 平台层 bInitialized: 测试只需注入 token,
	// 把 init flag 置 true 会让真实游戏后续 Initialize() 早退、ClientID 拿不到, 反而引入 bug。
	// BuildAuthorizationHeader 已改为只校验 PlatformImpl.IsValid(), 不再依赖 IsInitialized()。
	PlatformImpl->InjectTokenForTest(Kid, AccessToken, MacKey, MacAlgorithm, Scopes);
}
#endif
